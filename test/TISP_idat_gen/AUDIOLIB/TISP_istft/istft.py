import numpy as np
import torch
import scipy.signal


# Maps CSV windowType string to TISP::WindowType enum uint32_t value
# (identical set to TISP_stft's WINDOW_TYPE_ENUM)
WINDOW_TYPE_ENUM = {
    "rectangular": 0,
    "hann": 1,
    "hamming": 2,
    "blackman": 3,
    "blackman_harris": 4,
    "bartlett": 5,
    "sqrt(hann)": 6,
}


class TISP_istft:
    """ISTFT test data generator class.

    Generates complex spectral input and expected real-sample output for ISTFT
    test cases.  The reference is the per-frame inverse FFT + synthesis window +
    overlap-add pipeline that AUDIOLIB_istft_cn implements (NOT torch.istft,
    which additionally normalizes by the squared-window overlap).  This mirrors
    the role TISP_stft plays for the forward transform, where torch.stft is the
    authoritative reference.
    """

    def __init__(self, fftSize=512, windowLength=None, hopSize=256, numChannels=1, numWindows=4,
                 onesided=True, numBlocks=1, windowType="hann", align="center", periodic=True):
        """Initialize ISTFT processor.

        Args:
            fftSize: FFT size (N)
            hopSize: Hop size (H)
            numChannels: Number of channels (M)
            numWindows: Spectral frames per exec() call (W)
            onesided: If True, input is half spectrum (N/2+1 bins)
            numBlocks: Number of blocks (exec() calls)
            windowType: Window type string (key in WINDOW_TYPE_ENUM)
        """
        self.fftSize = fftSize
        self.windowLength = fftSize if windowLength is None else windowLength
        self.hopSize = hopSize
        self.numChannels = numChannels
        self.numWindows = numWindows
        self.onesided = onesided
        self.numBlocks = numBlocks
        self.windowType = windowType
        self.align = align
        self.periodic = periodic
        # synthesis window: windowLength coefficients, placed (center/left) within fftSize
        self.window = generate_padded_window(windowType, self.windowLength, fftSize, align, periodic)

        # Compute derived constants (mirrors AUDIOLIB_istft_cn / TISP_istft.cpp)
        if onesided:
            self.inBins = fftSize + 2          # (N/2+1) complex pairs = N+2 floats
        else:
            self.inBins = 2 * fftSize          # N complex pairs = 2*N floats

        self.overlapBufSize = fftSize - hopSize
        self.numOutputSamples = numWindows * hopSize

    def exec(self, inputData):
        """Execute ISTFT on multi-block complex spectral input.

        Args:
            inputData: complex interleaved float32 array, block-major layout
                       [numBlocks * numChannels * numWindows * inBins]

        Returns:
            output: real float32 array
                    [numBlocks * numChannels * numOutputSamples]
        """
        return istft_reference(
            inputData, self.window, self.fftSize, self.hopSize,
            self.numChannels, self.numWindows, self.onesided, self.numBlocks
        )


def generate_window(window_type_str, fft_size, periodic=True):
    """Generate a synthesis window as a float32 numpy array.

    Uses the same torch/scipy functions as TISP_stft.generate_window (which
    mirror TISP::windowGen()) so the ISTFT reference and the forward STFT
    reference produce bit-identical window coefficients for the same window
    type and symmetry.  periodic=True selects the DFT-even form (denominator N,
    torch periodic / scipy fftbins=True); periodic=False selects the symmetric
    form (denominator N-1, torch periodic=False / scipy sym=True).  Bartlett is
    always symmetric (numpy.bartlett) and ignores periodic.  The synthesis
    window spans the full fftSize frame, so the length argument is fft_size (the
    forward STFT's analysis window may instead be a shorter windowLength,
    center-padded to fftSize).
    """
    if window_type_str == "rectangular":
        return np.ones(fft_size, dtype=np.float32)
    elif window_type_str == "hann":
        return torch.hann_window(fft_size, periodic=periodic).numpy().astype(np.float32)
    elif window_type_str == "hamming":
        return torch.hamming_window(fft_size, periodic=periodic).numpy().astype(np.float32)
    elif window_type_str == "blackman":
        return torch.blackman_window(fft_size, periodic=periodic).numpy().astype(np.float32)
    elif window_type_str == "blackman_harris":
        return scipy.signal.windows.blackmanharris(fft_size, sym=not periodic).astype(np.float32)
    elif window_type_str == "bartlett":
        return np.bartlett(fft_size).astype(np.float32)
    elif window_type_str == "sqrt(hann)":
        hann = torch.hann_window(fft_size, periodic=periodic).numpy().astype(np.float32)
        return np.sqrt(hann).astype(np.float32)
    else:
        raise ValueError("Unknown window type: %s" % window_type_str)


def generate_padded_window(window_type_str, window_length, fft_size, align="center", periodic=True):
    """Generate a synthesis window of length window_length placed in an fft_size buffer.

    Matches TISP::windowGen()'s placement convention:
      "center" -> pad_left = (fft_size - window_length) // 2  (PyTorch / librosa)
      "left"   -> pad_left = 0, zeros on the right  (scipy / conv-STFT, e.g. DCCRN)
    When window_length == fft_size the placement is irrelevant.  periodic selects
    DFT-even (True) vs symmetric (False) coefficients.
    """
    if window_length > fft_size:
        raise ValueError(
            "window_length (%d) must not exceed fft_size (%d)" % (window_length, fft_size)
        )
    win = generate_window(window_type_str, window_length, periodic)
    if window_length == fft_size:
        return win
    padded = np.zeros(fft_size, dtype=np.float32)
    pad_left = 0 if align == "left" else (fft_size - window_length) // 2
    padded[pad_left : pad_left + window_length] = win
    return padded


def istft_reference(
    spec_flat, window, fft_size, hop_size, num_channels, num_windows, onesided, num_blocks
):
    """Compute ISTFT reference output from complex interleaved spectral input.

    Matches AUDIOLIB_istft_cn exactly: per-frame irfft/ifft + synthesis window + OLA,
    then the torch.istft window-envelope normalization (always applied): each emitted
    sample is divided by the steady-state envelope sum_i window[j+i*hop]^2 (+eps).
    OLA state is carried across blocks (zero-initialized at start).

    Input layout  (float32 interleaved): [numBlocks, numChannels, numWindows, inBins]
      where inBins = fftSize+2 (onesided) or 2*fftSize (twosided)
    Output layout (float32): [numBlocks, numChannels, numOutputSamples]
      where numOutputSamples = numWindows * hopSize

    Args:
        spec_flat: complex interleaved float32 array (flat, block-major)
        window: synthesis window float32 array [fftSize]
        fft_size: FFT length
        hop_size: hop size
        num_channels: number of audio channels
        num_windows: spectral frames per exec() call
        onesided: True = half spectrum (N/2+1 bins), False = full spectrum
        num_blocks: number of exec() calls

    Returns:
        float32 output array, shape [numBlocks * numChannels * numOutputSamples]
    """
    if onesided:
        in_bins = fft_size + 2  # (N/2+1) complex pairs = (fft_size+2) floats
    else:
        in_bins = 2 * fft_size

    overlap_buf_size = fft_size - hop_size
    num_output_samples = num_windows * hop_size

    out_flat = np.zeros(
        num_blocks * num_channels * num_output_samples, dtype=np.float32
    )

    # steady-state window envelope for torch.istft-style normalization (mirrors
    # AUDIOLIB_istft_cn): win_envelope[j] = sum_i window[j + i*hop_size]^2, j in [0,hop)
    norm_eps = np.float32(1e-8)
    win_envelope = np.zeros(hop_size, dtype=np.float32)
    for j in range(hop_size):
        win_envelope[j] = np.sum(window[j:fft_size:hop_size] ** 2)

    for ch_idx in range(num_channels):
        ola_state = np.zeros(overlap_buf_size, dtype=np.float32)

        for block_idx in range(num_blocks):
            for frame_idx in range(num_windows):
                # offset into spec_flat for this frame
                frame_offset = (
                    block_idx * num_channels * num_windows * in_bins
                    + ch_idx * num_windows * in_bins
                    + frame_idx * in_bins
                )
                frame_data = spec_flat[frame_offset : frame_offset + in_bins]

                # unpack interleaved floats to complex array
                spec = (frame_data[0::2] + 1j * frame_data[1::2]).astype(np.complex64)

                if onesided:
                    # irfft: normalized IDFT on reconstructed Hermitian-symmetric spectrum
                    time_frame = np.fft.irfft(spec, n=fft_size).astype(np.float32)
                else:
                    # ifft: normalized IDFT, take real part (matches ifftDit behavior)
                    time_frame = np.fft.ifft(spec).real.astype(np.float32)

                # apply synthesis window
                time_frame = time_frame * window

                # overlap-add: load state, accumulate windowed frame, emit, save
                work_buf = np.zeros(fft_size, dtype=np.float32)
                if overlap_buf_size > 0:
                    work_buf[:overlap_buf_size] = ola_state
                work_buf += time_frame

                # write hopSize output samples
                out_offset = (
                    block_idx * num_channels * num_output_samples
                    + ch_idx * num_output_samples
                    + frame_idx * hop_size
                )
                out_flat[out_offset : out_offset + hop_size] = work_buf[:hop_size] / (win_envelope + norm_eps)

                # save new OLA state (samples hopSize..fftSize-1)
                if overlap_buf_size > 0:
                    ola_state = work_buf[hop_size:fft_size].copy()

    return out_flat


def generate_hermitian_input(fft_size, onesided, total_frames, rng):
    """Generate complex interleaved spectral input with guaranteed Hermitian symmetry.

    Input spectra are produced by forward-FFT of a random real signal.  FFTLIB's
    split-trick IFFT and numpy irfft produce identical results only for
    Hermitian-symmetric input; random complex spectra introduce a ~1-3 %
    systematic mismatch between the two implementations.

    Args:
        fft_size: FFT length
        onesided: True = half spectrum (N/2+1 bins), False = full spectrum (N bins)
        total_frames: numBlocks * numChannels * numWindows
        rng: a callable returning a real signal of the requested length, e.g.
             lambda count: np.random.uniform(-1.0, 1.0, count).astype(np.float32)

    Returns:
        float32 complex-interleaved array [total_frames * inBins]
    """
    in_bins = (fft_size + 2) if onesided else (2 * fft_size)
    real_signal = rng(total_frames * fft_size)
    x = np.zeros(total_frames * in_bins, dtype=np.float32)

    for idx in range(total_frames):
        frame = real_signal[idx * fft_size : (idx + 1) * fft_size]
        if onesided:
            # rfft output is Hermitian-symmetric by construction: N/2+1 complex bins
            spec = np.fft.rfft(frame).astype(np.complex64)
        else:
            # fft output of a real signal has Hermitian symmetry across all N bins
            spec = np.fft.fft(frame).astype(np.complex64)
        offset = idx * in_bins
        x[offset : offset + in_bins] = np.stack([spec.real, spec.imag], axis=1).flatten()

    return x
