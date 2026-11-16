# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class UndB20:
    """Reference model of the undB20 kernel: 10 ** (x / 20)."""

    DIVISOR = 20

    def __init__(self, *, num_channels, num_samples, num_blocks, dtype, min_x, max_x, **_):
        self.num_channels = int(num_channels)
        self.num_samples = int(num_samples)
        self.num_blocks = int(num_blocks)
        self.dtype = dtype  # only "float" is supported (as in the original gen_data)
        self.min_x = min_x
        self.max_x = max_x
        self.init()

    def init(self):
        if self.dtype != "float":
            raise ValueError("unsupported dtype: %s" % self.dtype)
        self.fmax = np.finfo(np.float32).max
        self.ftiny = np.finfo(np.float32).tiny

    def generate_inputs(self):
        """Test-data only: uniform random float32 input, shape (channels*blocks, samples)."""
        length = self.num_blocks * self.num_channels * self.num_samples
        in0 = np.random.uniform(self.min_x, self.max_x, length).astype(np.float32)
        return in0.reshape(self.num_channels * self.num_blocks, self.num_samples)

    def exec(self, x):
        """Kernel reference compute."""
        out = np.power(10, x / self.DIVISOR)
        out[out > self.fmax] = self.fmax
        out[out < self.ftiny] = 0.0
        return out
