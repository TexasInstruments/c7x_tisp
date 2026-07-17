"""Log-mel spectrogram reference generator for the TISP AUDIOLIB logMelSpectrum kernel.

This mirrors the structure of ``TISP_stft/stft.py``.  The log-mel spectrogram is
the front-end feature extraction shared by the yamnet-audioset and audioai-vggish
models being ported to the C7x.  The pipeline is:

    waveform --STFT--> complex frames --|.|^power--> magnitude/power spectrogram
             --mel filterbank matmul--> mel spectrogram
             --log(x + offset)--> log-mel spectrogram

The STFT stage reuses the exact streaming convention of ``TISP_stft`` (torch.stft
with ``center=False`` plus a zero-initialised overlap state prepended to the
signal), so a logMelSpectrum kernel that composes the existing ``TISP::AUDIOLIB::Stft``
node reproduces this reference bit-for-bit.  The mel filterbank uses
``torchaudio.functional.melscale_fbanks`` as the authoritative matrix (this is the
same matrix ``torchaudio.transforms.MelScale`` uses internally, which is what the
audioai-vggish pipeline calls).

One code path serves both models: ``torch_log_mel()`` below is literally
``torchaudio.transforms.MelSpectrogram(**cfg) + torch.log(x + offset)`` driven by a
config dict, and it reproduces both target models exactly (see VGGISH_CONFIG /
YAMNET_CONFIG).  The numpy pipeline (``log_mel_reference``) mirrors that call and is
the golden the C kernel must match; ``_run_self_check`` validates numpy-vs-torch for
each preset.

Model-specific parameter sets (both are the SAME torchaudio call, different kwargs):

  audioai-vggish : sr=16000 n_fft=1024 hop=512 winLen=1024 nMels=64 power=2.0
                   fMin=0    fMax=None melScale='htk' norm=None logOffset=1e-9
                   center=True   (utils/audio_processing.py MelSpectrogram(center=True))
  yamnet-audioset: sr=16000 n_fft=512  hop=160 winLen=400  nMels=64 power=1.0
                   fMin=125  fMax=7500 melScale='htk' norm=None logOffset=1e-3
                   center=True   (torch_audioset VGGishLogMelSpectrogram; MelSpectrogram
                                  default center=True, periodic Hann)

Both models use torchaudio's ``center=True`` (reflect-pad n_fft//2 both sides).
yamnet computes ``spectrogram(power=2)**0.5``, which is identical to ``power=1.0`` —
so a single ``power`` knob covers it.  ``center=False`` is still supported for the
streaming/overlap layout (matching the TISP Stft node) but reproduces NEITHER model
bit-exactly (it differs at the first frame and drops the trailing frame).
"""

import numpy as np
import torch
import torchaudio
import scipy.signal


# Maps CSV windowType string to TISP::WindowType enum uint32_t value.
# Kept identical to TISP_stft/stft.py so the two generators stay in sync.
WINDOW_TYPE_ENUM = {
    "rectangular": 0,
    "hann": 1,
    "hamming": 2,
    "blackman": 3,
    "blackman_harris": 4,
    "bartlett": 5,
    "sqrt(hann)": 6,
}

# Maps CSV melScale string to a TISP-side enum uint32_t value.
MEL_SCALE_ENUM = {
    "htk": 0,
    "slaney": 1,
}


# Authoritative parameter presets for the two target models.  Both are the SAME
# torchaudio.transforms.MelSpectrogram call (see torch_log_mel) with center=True;
# only the kwargs differ.  These keys are a subset of the TISP_logMelSpectrum
# constructor kwargs, so ``TISP_logMelSpectrum(**VGGISH_CONFIG, ...)`` works too.
VGGISH_CONFIG = dict(   # audioai-vggish/utils/audio_processing.py
    fftSize=1024, windowLength=1024, hopSize=512, numMelBins=64,
    sampleRate=16000, fMin=0.0, fMax=None, power=2.0, logOffset=1e-9,
    melScale="htk", norm=None, windowType="hann", align="center",
    periodic=True, center=True,
)
YAMNET_CONFIG = dict(   # yamnet-audioset torch_audioset VGGishLogMelSpectrogram + CommonParams
    fftSize=512, windowLength=400, hopSize=160, numMelBins=64,
    sampleRate=16000, fMin=125.0, fMax=7500.0, power=1.0, logOffset=1e-3,
    melScale="htk", norm=None, windowType="hann", align="center",
    periodic=True, center=True,
)   # power=1.0 == yamnet's spectrogram(power=2)**0.5

# Named presets addressable from the CSV via an optional "model" column.
MODEL_CONFIGS = {"vggish": VGGISH_CONFIG, "yamnet": YAMNET_CONFIG}

# Maps a windowType string to the torchaudio window constructor for torch_log_mel.
_TORCH_WINDOW_FN = {
    "hann": torch.hann_window,
    "hamming": torch.hamming_window,
    "blackman": torch.blackman_window,
}


class TISP_logMelSpectrum:
    """Log-mel spectrogram test data generator.

    Generates input (waveform) and expected output (log-mel spectrogram) for
    logMelSpectrum test cases, using torch.stft + torchaudio mel filterbank as
    the reference implementation.
    """

    def __init__(self, fftSize=512, windowLength=400, hopSize=160,
                 numChannels=1, numInputSamples=1600, numMelBins=64,
                 sampleRate=16000, fMin=125.0, fMax=7500.0, power=1.0,
                 logOffset=1e-3, melScale="htk", norm=None,
                 numBlocks=1, windowType="hann", align="center",
                 periodic=True):
        """Initialise the log-mel spectrogram processor.

        Args:
            fftSize:         FFT size (N).
            windowLength:    Analysis window length (<= fftSize).
            hopSize:         Hop size (H).
            numChannels:     Number of channels (M).
            numInputSamples: Single-block input size (F).
            numMelBins:      Number of mel bands in the output (nMels).
            sampleRate:      Audio sample rate in Hz.
            fMin:            Lower edge of the lowest mel band, Hz.
            fMax:            Upper edge of the highest mel band, Hz.
            power:           Exponent on the STFT magnitude (1.0=magnitude, 2.0=power).
            logOffset:       Offset added before the log (log(mel + logOffset)).
            melScale:        "htk" or "slaney" mel-frequency warping.
            norm:            None or "slaney" filterbank area normalisation.
            numBlocks:       Number of streaming blocks.
            windowType:      Window type string (key in WINDOW_TYPE_ENUM).
            align:           "center" or "left" window placement inside fftSize.
            periodic:        DFT-even (True) vs symmetric (False) window.

        This processor is streaming-only (unconsumed-tail overlap model, matching
        the TISP Stft node and the LogMelSpectrum kernel).  Reflect-pad "center"
        alignment is intentionally not modelled here: it is a whole-clip concept
        the caller handles off-device.  ``torch_log_mel`` (below) is kept as the
        host-only ground truth for the real center=True model features.
        """
        self.fftSize = fftSize
        self.windowLength = windowLength
        self.hopSize = hopSize
        self.numChannels = numChannels
        self.numInputSamples = numInputSamples
        self.numMelBins = numMelBins
        self.sampleRate = sampleRate
        self.fMin = fMin
        self.fMax = fMax
        self.power = power
        self.logOffset = logOffset
        self.melScale = melScale
        self.norm = norm
        self.numBlocks = numBlocks
        self.windowType = windowType
        self.align = align
        self.periodic = periodic
        self.window = generate_window(windowType, windowLength, periodic)

        # torchaudio treats f_max=None as sampleRate/2 (Nyquist).  audioai-vggish
        # does not pass f_max, so resolve None the same way here.
        if self.fMax is None:
            self.fMax = sampleRate / 2.0

        # number of unique (onesided) spectrogram bins feeding the mel matrix
        self.numSpectrogramBins = fftSize // 2 + 1

        # mel filterbank: (numSpectrogramBins, numMelBins), reused across frames
        self.melMatrix = mel_filterbank(
            self.numSpectrogramBins, numMelBins, sampleRate,
            fMin, self.fMax, melScale, norm
        )

        # Derived STFT frame count: streaming unconsumed-tail overlap model,
        # identical formula to TISP_stft/stft.py.
        n = fftSize
        h = hopSize
        f = numInputSamples
        firstFrameWindows = (f - n) // h + 1
        self.overlapBufSize = f - firstFrameWindows * h
        self.numWindows = (self.overlapBufSize + f - n) // h + 1

        # log-mel output has numMelBins real values per frame (vs outBins for STFT)
        self.outMel = numMelBins

    def exec(self, inputData):
        """Execute log-mel spectrogram on multi-block, multi-channel input.

        Args:
            inputData: Input signal [numBlocks * numInputSamples * numChannels]
                       in block-major, channel-contiguous layout.

        Returns:
            output: log-mel spectrogram
                    [numBlocks * numWindows * numMelBins * numChannels]
                    in the same block-major, channel-contiguous layout.
        """
        blockInputSize = self.numInputSamples * self.numChannels
        blockOutputSize = self.numWindows * self.outMel * self.numChannels
        outputLen = self.numBlocks * blockOutputSize
        output = np.zeros(outputLen, dtype=np.float32)

        for chIdx in range(self.numChannels):
            # gather all blocks for this channel into one contiguous signal
            xFullCh = np.zeros(self.numBlocks * self.numInputSamples, dtype=np.float32)
            for blockIdx in range(self.numBlocks):
                srcOffset = blockIdx * blockInputSize + chIdx * self.numInputSamples
                xFullCh[blockIdx * self.numInputSamples:(blockIdx + 1) * self.numInputSamples] = (
                    inputData[srcOffset:srcOffset + self.numInputSamples]
                )

            # full-signal log-mel spectrogram -> (numBlocks * numWindows, numMelBins)
            yFullCh = log_mel_reference(
                xFullCh, self.window, self.melMatrix, self.fftSize,
                self.windowLength, self.hopSize, self.numInputSamples,
                self.power, self.logOffset, self.align
            )

            # split into blocks and place in block-major layout
            for blockIdx in range(self.numBlocks):
                srcOffset = blockIdx * self.numWindows * self.outMel
                dstOffset = blockIdx * blockOutputSize + chIdx * self.numWindows * self.outMel
                output[dstOffset:dstOffset + self.numWindows * self.outMel] = (
                    yFullCh[srcOffset:srcOffset + self.numWindows * self.outMel]
                )

        return output


def generate_window(window_type_str, window_length, periodic=True):
    """Generate a window as a float32 numpy array.

    Identical to TISP_stft/stft.py::generate_window so both kernels share window
    semantics.  periodic=True selects the DFT-even form (denominator N, matching
    torch.*_window(periodic=True)); periodic=False selects the symmetric form.
    """
    if window_type_str == "rectangular":
        return np.ones(window_length, dtype=np.float32)
    elif window_type_str == "hann":
        return torch.hann_window(window_length, periodic=periodic).numpy().astype(np.float32)
    elif window_type_str == "hamming":
        return torch.hamming_window(window_length, periodic=periodic).numpy().astype(np.float32)
    elif window_type_str == "blackman":
        return torch.blackman_window(window_length, periodic=periodic).numpy().astype(np.float32)
    elif window_type_str == "blackman_harris":
        return scipy.signal.windows.blackmanharris(window_length, sym=not periodic).astype(np.float32)
    elif window_type_str == "bartlett":
        return np.bartlett(window_length).astype(np.float32)
    elif window_type_str == "sqrt(hann)":
        hann = torch.hann_window(window_length, periodic=periodic).numpy().astype(np.float32)
        return np.sqrt(hann).astype(np.float32)
    else:
        raise ValueError("Unknown window type: %s" % window_type_str)


def place_window(window_np, fft_size, align="center"):
    """Place a window_length window into a full fft_size buffer.

    Mirrors TISP_stft/stft.py::place_window:
      "center" -> pad_left = (fft_size - window_length) // 2 (torch.stft default
                  for win_length < n_fft / TISP::windowGen center overload)
      "left"   -> pad_left = 0 (zeros on the right).
    """
    wl = len(window_np)
    if wl > fft_size:
        raise ValueError("window_length (%d) must not exceed fft_size (%d)" % (wl, fft_size))
    if wl == fft_size:
        return window_np.astype(np.float32)
    full = np.zeros(fft_size, dtype=np.float32)
    pad_left = 0 if align == "left" else (fft_size - wl) // 2
    full[pad_left:pad_left + wl] = window_np
    return full


def mel_filterbank(num_spectrogram_bins, num_mel_bins, sample_rate,
                   f_min, f_max, mel_scale="htk", norm=None):
    """Return the (num_spectrogram_bins, num_mel_bins) mel weight matrix.

    Uses torchaudio.functional.melscale_fbanks, the same matrix that
    torchaudio.transforms.MelScale applies internally.  Post-multiplying an
    (frames, num_spectrogram_bins) magnitude spectrogram by this matrix yields
    an (frames, num_mel_bins) mel spectrogram, matching the audioai-vggish and
    yamnet feature front-ends.

    Returns:
        float32 numpy array of shape (num_spectrogram_bins, num_mel_bins).
    """
    fb = torchaudio.functional.melscale_fbanks(
        n_freqs=num_spectrogram_bins,
        f_min=float(f_min),
        f_max=float(f_max),
        n_mels=num_mel_bins,
        sample_rate=sample_rate,
        norm=norm,
        mel_scale=mel_scale,
    )
    return fb.numpy().astype(np.float32)


def stft_complex_frames(signal, window_np, fft_size, window_length, hop_size,
                        num_input_samples, align="center"):
    """Compute the onesided STFT as a complex (frames, bins) array.

    Reproduces the TISP_stft streaming model: a zero-initialised overlap tail is
    prepended to the signal so the first exec() sees zeros, and torch.stft is
    called with center=False.  (Reflect-pad "center" alignment is a whole-clip
    concept handled off-device; see torch_log_mel.)

    Returns:
        complex64 numpy array of shape (num_frames, fft_size // 2 + 1).
    """
    n = fft_size
    h = hop_size
    f = num_input_samples

    # streaming path: prepend zeros(overlapBufSize) exactly as TISP_stft does.
    firstFrameWindows = (f - n) // h + 1
    overlapBufSize = f - firstFrameWindows * h
    padded = np.concatenate([np.zeros(overlapBufSize, dtype=np.float32), signal])
    sig_t = torch.from_numpy(padded)

    if align == "left":
        win_t = torch.from_numpy(place_window(window_np, fft_size, "left"))
        win_len_arg = fft_size
    else:
        win_t = torch.from_numpy(window_np)
        win_len_arg = window_length

    result = torch.stft(
        sig_t,
        n_fft=fft_size,
        hop_length=hop_size,
        win_length=win_len_arg,
        window=win_t,
        center=False,
        return_complex=True,
        onesided=True,
    )
    # (bins, frames) -> (frames, bins)
    return result.T.numpy().astype(np.complex64)


def log_mel_reference(signal, window_np, mel_matrix, fft_size, window_length,
                      hop_size, num_input_samples, power=1.0, log_offset=1e-3,
                      align="center"):
    """Compute the flattened log-mel spectrogram for a single-channel signal.

    Streaming pipeline: STFT -> |.|^power magnitude/power spectrogram -> mel
    filterbank matmul -> log(mel + log_offset).

    Returns:
        float32 numpy array of length num_frames * num_mel_bins, row-major
        (frame-major: [frame0(nMels) | frame1(nMels) | ...]).
    """
    frames = stft_complex_frames(
        signal, window_np, fft_size, window_length, hop_size,
        num_input_samples, align
    )

    # magnitude / power spectrogram: |z| ** power, shape (frames, bins)
    magnitude = np.abs(frames).astype(np.float32) ** np.float32(power)

    # mel spectrogram: (frames, bins) @ (bins, nMels) -> (frames, nMels)
    mel_spec = magnitude @ mel_matrix

    # stabilised log
    log_mel = np.log(mel_spec + np.float32(log_offset)).astype(np.float32)

    return log_mel.reshape(-1)


# ---------------------------------------------------------------------------
# Single authoritative torch log-mel: torchaudio.transforms.MelSpectrogram + log,
# driven by a config dict.  This IS "torch log melspectrum" and reproduces BOTH
# target models (audioai-vggish and yamnet-audioset) with only kwargs changing.
# ---------------------------------------------------------------------------
def torch_log_mel(waveform, cfg):
    """Authoritative torch log-mel spectrogram for any model config.

    Equivalent to audioai-vggish's ``log(MelSpectrogram(center=True)(x) + 1e-9)``
    and to yamnet-audioset's ``VGGishLogMelSpectrogram`` (its
    ``spectrogram(power=2)**0.5`` equals ``power=1.0`` here).

    Args:
        waveform: 1-D float32 torch tensor of samples.
        cfg:      one of the model config dicts (VGGISH_CONFIG / YAMNET_CONFIG).

    Returns:
        torch tensor of shape (numMelBins, numFrames) — log-mel spectrogram.
    """
    window_fn = _TORCH_WINDOW_FN.get(cfg["windowType"], torch.hann_window)
    mel = torchaudio.transforms.MelSpectrogram(
        sample_rate=cfg["sampleRate"], n_fft=cfg["fftSize"],
        win_length=cfg["windowLength"], hop_length=cfg["hopSize"],
        f_min=cfg["fMin"], f_max=cfg["fMax"], n_mels=cfg["numMelBins"],
        power=cfg["power"], center=cfg["center"], norm=cfg["norm"],
        mel_scale=cfg["melScale"], window_fn=window_fn,
        wkwargs={"periodic": cfg["periodic"]},
    )
    return torch.log(mel(waveform) + cfg["logOffset"])


# ---------------------------------------------------------------------------
# Self-checks.  Run directly:  python logMelSpectrum.py
#   1) validate torch_log_mel (the host-only model ground truth) against
#      torchaudio.transforms.MelSpectrogram + log for each preset (center=True).
#   2) report the streaming-vs-model gap: how far the streaming reference (the
#      on-device behaviour, center-less) is from the true center=True model.
# ---------------------------------------------------------------------------
def _check_model_reference(name, cfg):
    """torch_log_mel must equal a direct torchaudio MelSpectrogram(center=True)+log."""
    torch.manual_seed(0)
    wav = torch.rand(cfg["sampleRate"], dtype=torch.float32) * 2.0 - 1.0  # 1 s

    ours = torch_log_mel(wav, cfg).numpy()  # (nMels, frames)

    window_fn = _TORCH_WINDOW_FN.get(cfg["windowType"], torch.hann_window)
    mel = torchaudio.transforms.MelSpectrogram(
        sample_rate=cfg["sampleRate"], n_fft=cfg["fftSize"], win_length=cfg["windowLength"],
        hop_length=cfg["hopSize"], f_min=cfg["fMin"], f_max=cfg["fMax"], n_mels=cfg["numMelBins"],
        power=cfg["power"], center=cfg["center"], norm=cfg["norm"], mel_scale=cfg["melScale"],
        window_fn=window_fn, wkwargs={"periodic": cfg["periodic"]},
    )
    ref = torch.log(mel(wav) + cfg["logOffset"]).numpy()
    d = float(np.max(np.abs(ours - ref)))
    print("model-reference check (%s, center=%s): frames=%d n_mels=%d  max diff = %.3e  %s"
          % (name, cfg["center"], ref.shape[1], cfg["numMelBins"], d, "PASS" if d < 1e-4 else "FAIL"))
    assert d < 1e-4, "%s: torch_log_mel != torchaudio (%.3e)" % (name, d)


def _report_streaming_gap(name, cfg):
    """Report how far the streaming reference is from the true center=True model.

    The streaming framing only aligns with center=True when hopSize == fftSize/2
    (e.g. vggish); otherwise the whole spectrogram is time-shifted (e.g. yamnet),
    so this gap is intentionally informational, not a pass/fail gate.
    """
    torch.manual_seed(0)
    num_samples = cfg["sampleRate"]  # 1 s
    wav = torch.rand(num_samples, dtype=torch.float32) * 2.0 - 1.0

    model = torch_log_mel(wav, cfg).numpy().T  # (frames, nMels), center=True

    fMax = cfg["fMax"] if cfg["fMax"] is not None else cfg["sampleRate"] / 2.0
    window = generate_window(cfg["windowType"], cfg["windowLength"], cfg["periodic"])
    mel_matrix = mel_filterbank(cfg["fftSize"] // 2 + 1, cfg["numMelBins"], cfg["sampleRate"],
                                cfg["fMin"], fMax, cfg["melScale"], cfg["norm"])
    stream = log_mel_reference(
        wav.numpy(), window, mel_matrix, cfg["fftSize"], cfg["windowLength"], cfg["hopSize"],
        num_input_samples=num_samples, power=cfg["power"], log_offset=cfg["logOffset"],
        align=cfg["align"],
    ).reshape(-1, cfg["numMelBins"])

    nf = min(model.shape[0], stream.shape[0])
    gap = float(np.max(np.abs(model[:nf] - stream[:nf])))
    aligned = (cfg["hopSize"] == cfg["fftSize"] // 2)
    print("streaming-vs-model gap (%s): stream frames=%d, model frames=%d, max gap=%.3e  (%s)"
          % (name, stream.shape[0], model.shape[0], gap,
             "hop==fftSize/2, interior aligns" if aligned else "hop!=fftSize/2, time-shifted"))


if __name__ == "__main__":
    _check_model_reference("audioai-vggish", VGGISH_CONFIG)
    _check_model_reference("yamnet-audioset", YAMNET_CONFIG)
    print()
    _report_streaming_gap("audioai-vggish", VGGISH_CONFIG)
    _report_streaming_gap("yamnet-audioset", YAMNET_CONFIG)
