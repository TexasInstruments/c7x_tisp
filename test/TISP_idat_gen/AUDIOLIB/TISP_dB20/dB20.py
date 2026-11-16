# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class DB20:
    """Reference model of the dB20 kernel: 20 * log10(x)."""

    SCALE = 20

    def __init__(self, *, num_channels, num_samples, num_blocks, dtype, min_x, max_x, **_):
        self.num_channels = int(num_channels)
        self.num_samples = int(num_samples)
        self.num_blocks = int(num_blocks)
        self.dtype = dtype  # CSV dType string: "float" or otherwise double
        self.min_x = min_x
        self.max_x = max_x
        self.init()

    def init(self):
        self.is_float = self.dtype == "float"
        self.np_dtype = np.float32 if self.is_float else np.float64

    def generate_inputs(self):
        """Test-data only: uniform random input of shape (channels*blocks, samples)."""
        length = self.num_blocks * self.num_channels * self.num_samples
        return (
            np.random.uniform(self.min_x, self.max_x, length)
            .astype(self.np_dtype)
            .reshape(self.num_channels * self.num_blocks, self.num_samples)
        )

    def exec(self, x):
        """Kernel reference compute."""
        out = np.log10(x)

        if self.is_float:
            out[x > np.finfo(np.float32).max] = 308.2547
            out[x <= 0.0] = np.uint32(0xFF800000)
        else:
            out[x > np.finfo(np.float64).max] = 308.2547
            out[x <= 0.0] = np.uint64(
                0xFFF0000000000000
            )  # NaN/-Inf bit pattern for float64

        return out * self.SCALE
