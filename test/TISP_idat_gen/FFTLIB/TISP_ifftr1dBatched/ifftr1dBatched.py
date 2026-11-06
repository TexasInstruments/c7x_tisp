# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np

# IFFT algorithm for complex-to-real: generates real output from complex input
# Batched version (multiple channels)
# fullSpectrum = 0 -> half spectrum input
# fullSpectrum = 1 -> full spectrum input


class Ifftr1dBatched:
    """IFFT algorithm: batched complex input (interleaved re/im float32) -> real output"""

    def __init__(self, *, num_channels, fft_size, fullSpectrum=1, num_blocks=1, **_):
        """
        Args:
            num_channels (int): Number of parallel IFFTs
            fft_size (int): IFFT size
            num_blocks (int): Independent frames run back to back. generate_inputs() returns them stacked
                along the channel axis ([num_blocks * num_channels, ...]); compute()/exec() take one frame
            fullSpectrum (int): 0=half spectrum input, 1=full spectrum input
        """
        self.num_channels = int(num_channels)
        self.fft_size = int(fft_size)
        self.fullSpectrum = fullSpectrum
        self.num_blocks = int(num_blocks)
        self.init()

    def init(self):
        """Pick the IFFT variant once (called by the constructor)."""
        if not self.fullSpectrum:
            self._ifft = lambda G: np.fft.irfft(G, n=self.fft_size, axis=1)
        else:
            self._ifft = lambda G: np.fft.ifft(G, axis=1).real

    def compute(self, G):
        """Real [num_channels, fft_size] output of a complex [num_channels, bins] spectrum.

        Used by exec() and by gen_data.py, which rounds the full-precision result for
        the test vectors.
        """
        return self._ifft(G)

    def exec(self, x):
        """One frame: float32 [num_channels, 2 * bins] interleaved real/imaginary
        -> float32 [num_channels, fft_size]."""
        G = np.ascontiguousarray(x, dtype=np.float32).view(np.complex64)
        return self.compute(G.astype(np.complex128)).astype(np.float32)

    def generate_inputs(self, min_val=-1.0, max_val=1.0, decimals=6):
        """SDK test signal: the spectrum of a random real signal, as complex128 rounded to
        `decimals` (for gen_data.py only)."""
        real = np.round(
            np.random.uniform(low=min_val, high=max_val, size=(self.num_blocks * self.num_channels, self.fft_size)),
            decimals,
        )
        if self.fullSpectrum:
            return np.round(np.fft.fft(real), decimals)
        return np.round(np.fft.rfft(real), decimals)
