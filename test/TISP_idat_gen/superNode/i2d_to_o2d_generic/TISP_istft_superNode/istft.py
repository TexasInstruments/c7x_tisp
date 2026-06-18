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
    """ISTFT test data generator class (local copy of AUDIOLIB/TISP_istft/istft.py,
    kept per the superNode generator convention).

    Generates complex spectral input and expected real-sample output.  The
    reference is the per-frame inverse FFT + synthesis window + overlap-add +
    steady-state window-envelope normalization that AUDIOLIB_istft_cn /
    TISP_istft.cpp implement.
    """

    def __init__(self, fftSize=512, windowLength=None, hopSize=256, numChannels=1, numWindows=4,
                 onesided=True, numBlocks=1, windowType="hann", align="center", periodic=True):
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
        self.window = generate_padded_window(windowType, self.windowLength, fftSize, align, periodic)

        if onesided:
            self.inBins = fftSize + 2          # (N/2+1) complex pairs = N+2 floats
        else:
            self.inBins = 2 * fftSize          # N complex pairs = 2*N floats

        self.overlapBufSize = fftSize - hopSize
        self.numOutputSamples = numWindows * hopSize

    def exec(self, inputData):
        """Execute ISTFT on multi-block complex spectral input.

        inputData layout : [numBlocks, numChannels, numWindows, inBins] complex-interleaved
        returns          : [numBlocks, numChannels, numOutputSamples] real
        """
        return istft_reference(
            inputData, self.window, self.fftSize, self.hopSize,
            self.numChannels, self.numWindows, self.onesided, self.numBlocks
        )


def generate_window(window_type_str, fft_size, periodic=True):
    """Generate a synthesis window as a float32 numpy array (matches TISP::windowGen)."""
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

      "center" -> pad_left = (fft_size - window_length) // 2  (PyTorch / librosa)
      "left"   -> pad_left = 0, zeros on the right  (scipy / conv-STFT, e.g. DCCRN)
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
    """ISTFT reference: per-frame irfft/ifft + synthesis window + OLA + steady-state
    window-envelope normalization (matches AUDIOLIB_istft_cn / TISP_istft.cpp).

    Input layout  : [numBlocks, numChannels, numWindows, inBins] complex-interleaved
    Output layout : [numBlocks, numChannels, numOutputSamples], numOutputSamples = numWindows*hopSize
    OLA state is zero-initialized per channel and carried across blocks.
    """
    if onesided:
        in_bins = fft_size + 2
    else:
        in_bins = 2 * fft_size

    overlap_buf_size = fft_size - hop_size
    num_output_samples = num_windows * hop_size

    out_flat = np.zeros(num_blocks * num_channels * num_output_samples, dtype=np.float32)

    # steady-state window envelope: win_envelope[j] = sum_i window[j + i*hop]^2, j in [0,hop)
    norm_eps = np.float32(1e-8)
    win_envelope = np.zeros(hop_size, dtype=np.float32)
    for j in range(hop_size):
        win_envelope[j] = np.sum(window[j:fft_size:hop_size] ** 2)

    for ch_idx in range(num_channels):
        ola_state = np.zeros(overlap_buf_size, dtype=np.float32)

        for block_idx in range(num_blocks):
            for frame_idx in range(num_windows):
                frame_offset = (
                    block_idx * num_channels * num_windows * in_bins
                    + ch_idx * num_windows * in_bins
                    + frame_idx * in_bins
                )
                frame_data = spec_flat[frame_offset : frame_offset + in_bins]

                spec = (frame_data[0::2] + 1j * frame_data[1::2]).astype(np.complex64)

                if onesided:
                    time_frame = np.fft.irfft(spec, n=fft_size).astype(np.float32)
                else:
                    time_frame = np.fft.ifft(spec).real.astype(np.float32)

                time_frame = time_frame * window

                work_buf = np.zeros(fft_size, dtype=np.float32)
                if overlap_buf_size > 0:
                    work_buf[:overlap_buf_size] = ola_state
                work_buf += time_frame

                out_offset = (
                    block_idx * num_channels * num_output_samples
                    + ch_idx * num_output_samples
                    + frame_idx * hop_size
                )
                out_flat[out_offset : out_offset + hop_size] = work_buf[:hop_size] / (win_envelope + norm_eps)

                if overlap_buf_size > 0:
                    ola_state = work_buf[hop_size:fft_size].copy()

    return out_flat


def generate_hermitian_input(fft_size, onesided, total_frames, rng):
    """Generate complex-interleaved spectral input with guaranteed Hermitian symmetry
    (forward-FFT of a random real signal), so FFTLIB's split-trick IFFT and numpy
    irfft/ifft agree.  Returns float32 [total_frames * inBins]."""
    in_bins = (fft_size + 2) if onesided else (2 * fft_size)
    real_signal = rng(total_frames * fft_size)
    x = np.zeros(total_frames * in_bins, dtype=np.float32)

    for idx in range(total_frames):
        frame = real_signal[idx * fft_size : (idx + 1) * fft_size]
        if onesided:
            spec = np.fft.rfft(frame).astype(np.complex64)
        else:
            spec = np.fft.fft(frame).astype(np.complex64)
        offset = idx * in_bins
        x[offset : offset + in_bins] = np.stack([spec.real, spec.imag], axis=1).flatten()

    return x
