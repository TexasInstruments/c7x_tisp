import numpy as np
import torch
import scipy.signal


# Maps CSV windowType string to TISP::WindowType enum uint32_t value
WINDOW_TYPE_ENUM = {
    "rectangular": 0,
    "hann": 1,
    "hamming": 2,
    "blackman": 3,
    "blackman_harris": 4,
    "bartlett": 5,
    "sqrt(hann)": 6,
}


class TISP_stft:
    """STFT test data generator class.

    Generates input and expected output for STFT test cases using torch.stft
    as the reference implementation.
    """

    def __init__(self, fftSize=512, windowLength=512, hopSize=256,
                 numChannels=1, numInputSamples=512, onesided=True,
                 numBlocks=1, windowType="hann", align="center", periodic=True):
        """Initialize STFT processor.

        Args:
            fftSize: FFT size (N)
            windowLength: Analysis window length
            hopSize: Hop size (H)
            numChannels: Number of channels (M)
            numInputSamples: Single-block input size (F)
            onesided: If True, use one-sided spectrum
            numBlocks: Number of blocks
            windowType: Window type string (key in WINDOW_TYPE_ENUM)
        """
        self.fftSize = fftSize
        self.windowLength = windowLength
        self.hopSize = hopSize
        self.numChannels = numChannels
        self.numInputSamples = numInputSamples
        self.onesided = onesided
        self.numBlocks = numBlocks
        self.windowType = windowType
        self.align = align
        self.periodic = periodic
        self.window = generate_window(windowType, windowLength, periodic)

        # Compute derived constants
        n = fftSize
        h = hopSize
        f = numInputSamples
        firstFrameWindows = (f - n) // h + 1
        self.overlapBufSize = f - firstFrameWindows * h
        self.numWindows = (self.overlapBufSize + f - n) // h + 1

        if onesided:
            self.outBins = n + 2
        else:
            self.outBins = 2 * n

    def exec(self, inputData):
        """Execute STFT on multi-block input.

        Args:
            inputData: Input signal [numBlocks * numInputSamples * numChannels]

        Returns:
            output: STFT output [numBlocks * numWindows * outBins * numChannels]
        """
        blockInputSize = self.numInputSamples * self.numChannels
        blockOutputSize = self.numWindows * self.outBins * self.numChannels
        outputLen = self.numBlocks * blockOutputSize
        output = np.zeros(outputLen, dtype=np.float32)

        for chIdx in range(self.numChannels):
            # Gather all blocks for this channel
            xFullCh = np.zeros(self.numBlocks * self.numInputSamples, dtype=np.float32)
            for blockIdx in range(self.numBlocks):
                srcOffset = blockIdx * blockInputSize + chIdx * self.numInputSamples
                xFullCh[blockIdx * self.numInputSamples : (blockIdx + 1) * self.numInputSamples] = (
                    inputData[srcOffset : srcOffset + self.numInputSamples]
                )

            # Run STFT on full multi-block signal
            yFullCh = stft_reference_torch(
                xFullCh, self.window, self.fftSize, self.windowLength,
                self.hopSize, self.numInputSamples, self.onesided, self.align
            )

            # Split output into blocks and place in block-major layout
            for blockIdx in range(self.numBlocks):
                srcOffset = blockIdx * self.numWindows * self.outBins
                dstOffset = blockIdx * blockOutputSize + chIdx * self.numWindows * self.outBins
                output[dstOffset : dstOffset + self.numWindows * self.outBins] = (
                    yFullCh[srcOffset : srcOffset + self.numWindows * self.outBins]
                )

        return output


def generate_window(window_type_str, window_length, periodic=True):
    """Generate a window as a float32 numpy array.

    Uses the same formulas as TISP::windowGen().  periodic=True selects the
    DFT-even form (denominator N, matching torch.*_window(periodic=True) /
    scipy fftbins=True); periodic=False selects the symmetric form
    (denominator N-1, matching torch.*_window(periodic=False) / scipy sym=True).
    Bartlett is always symmetric (matches numpy.bartlett) and ignores periodic.
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

    Mirrors how the C side builds pWindow:
      "center" -> pad_left = (fft_size - window_length) // 2  (PyTorch / TISP::windowGen
                  center-padded overload; what torch.stft does for win_length < n_fft)
      "left"   -> pad_left = 0  (zeros on the right; DCCRN ConvSTFT convention,
                  i.e. memset + single-size windowGen on the C side)
    Returns a float32 array of length fft_size.
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


def stft_reference_torch(signal, window_np, fft_size, window_length, hop_size, num_input_samples,
                         onesided, align="center"):
    """
    Compute STFT reference using torch.stft on zero-state + input signal.

    Prepends zeros[overlapBufSize] to match the TISP streaming model where
    the first exec() call starts with zero-initialized state.  Supports
    multi-block signals: when len(signal) == numBlocks * num_input_samples,
    the output contains numBlocks * numWindows frames (equivalent to
    streaming block-by-block with state carry-over).

    Args:
        signal: input samples as numpy float32 array [numBlocks * num_input_samples]
        window_np: analysis window as numpy float32 array [window_length]
        fft_size: FFT length
        window_length: analysis window length (<= fft_size)
        hop_size: hop size
        num_input_samples: single-block input size F (for overlap computation)
        onesided: if True, output N/2+1 bins; else N bins

    Returns:
        output: complex interleaved floats as numpy float32 array
    """
    n = fft_size
    h = hop_size
    f = num_input_samples

    # unconsumed-tail overlap model (based on single-block F)
    firstFrameWindows = (f - n) // h + 1
    overlapBufSize = f - firstFrameWindows * h

    # prepend zeros to match zero-initialized state
    padded = np.concatenate([np.zeros(overlapBufSize, dtype=np.float32), signal])

    sig_t = torch.from_numpy(padded)

    if align == "left":
        # left-aligned: build the full fft_size window ourselves and pass
        # win_length = fft_size so torch applies it verbatim (no centering)
        win_t       = torch.from_numpy(place_window(window_np, fft_size, "left"))
        win_len_arg = fft_size
    else:
        # center: torch.stft center-pads win_length < n_fft (matches windowGen center overload)
        win_t       = torch.from_numpy(window_np)
        win_len_arg = window_length

    result = torch.stft(
        sig_t,
        n_fft=fft_size,
        hop_length=hop_size,
        win_length=win_len_arg,
        window=win_t,
        center=False,
        return_complex=True,
        onesided=onesided,
    )

    # result shape: (num_bins, num_frames)
    # transpose to (num_frames, num_bins) for interleaving
    result = result.T

    # interleave real and imaginary parts to flat float32
    resultNp = result.numpy()
    totalFrames = resultNp.shape[0]

    if onesided:
        outBins = n + 2  # (N/2+1) complex pairs = N+2 floats
    else:
        outBins = 2 * n  # N complex pairs = 2*N floats

    output = np.zeros(totalFrames * outBins, dtype=np.float32)
    for frameIdx in range(totalFrames):
        for binIdx in range(resultNp.shape[1]):
            output[frameIdx * outBins + 2 * binIdx] = resultNp[frameIdx, binIdx].real
            output[frameIdx * outBins + 2 * binIdx + 1] = resultNp[
                frameIdx, binIdx
            ].imag

    return output
