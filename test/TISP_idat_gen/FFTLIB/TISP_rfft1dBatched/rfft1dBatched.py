# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np

# FFT algorithm: takes real input, generates complex output
# Batched version (multiple channels)
# fullSpectrum = 0: half spectrum (N//2 + 1 values using rfft)
# fullSpectrum = 1: full spectrum (N values using fft)


class Rfft1dBatched:
    """FFT algorithm: batched real input -> complex output (interleaved re/im float32)"""

    def __init__(self, *, num_channels, fft_size, fullSpectrum=1, num_blocks=1, **_):
        """
        Args:
            num_channels (int): Number of parallel FFTs
            fft_size (int): FFT size
            num_blocks (int): Independent frames run back to back. generate_inputs() returns them stacked
                along the channel axis ([num_blocks * num_channels, ...]); compute()/exec() take one frame
            fullSpectrum (int): 0=half spectrum, 1=full spectrum
        """
        self.num_channels = int(num_channels)
        self.fft_size = int(fft_size)
        self.fullSpectrum = fullSpectrum
        self.num_blocks = int(num_blocks)
        self.init()

    def init(self):
        """Pick the FFT variant once (called by the constructor)."""
        if not self.fullSpectrum:
            self._fft = lambda x: np.fft.rfft(x.astype(np.float64), axis=1)
        else:
            self._fft = lambda x: np.fft.fft(x.astype(np.complex128), axis=1)

    def compute(self, x):
        """Double-precision complex spectrum of a real [num_channels, fft_size] array.

        Used by exec() and by gen_data.py, which rounds the full-precision result for
        the test vectors.
        """
        return self._fft(x)

    def exec(self, x):
        """One frame: float32 [num_channels, fft_size] -> float32 [num_channels, 2 * bins]
        with interleaved real/imaginary parts (bins = fft_size or fft_size // 2 + 1)."""
        G = self.compute(np.asarray(x))
        return np.ascontiguousarray(G.astype(np.complex64)).view(np.float32)

    def generate_inputs(self, min_val=-1.0, max_val=1.0, decimals=6):
        """SDK test signal: random real [num_blocks * num_channels, fft_size] input rounded to `decimals`
        (for gen_data.py only)."""
        return np.round(
            np.random.uniform(low=min_val, high=max_val, size=(self.num_blocks * self.num_channels, self.fft_size)),
            decimals,
        )
