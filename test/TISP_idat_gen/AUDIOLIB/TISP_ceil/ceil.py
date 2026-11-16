# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class Ceil:
    """Reference model of the ceil kernel (element-wise np.ceil)."""

    def __init__(self, *, num_channels, num_samples, num_blocks, dtype, is_interleave, **_):
        self.num_channels = int(num_channels)
        self.num_samples = int(num_samples)
        self.num_blocks = int(num_blocks)
        self.dtype = dtype  # CSV dType string, e.g. "float" / "double"
        self.is_interleave = bool(is_interleave)
        self.init()

    def init(self):
        """Precompute the input value range and numpy dtype."""
        self.min_val, self.max_val = -10, 10
        if self.dtype[0] == "f":
            self.np_dtype = "float32"
        elif self.dtype[0] == "d":
            self.np_dtype = "double"
        else:
            raise ValueError("unsupported dtype: %s" % self.dtype)

    def generate_inputs(self):
        """Test-data only: random input, transposed when interleaved."""
        in0 = np.random.uniform(
            low=self.min_val,
            high=self.max_val,
            size=(self.num_channels * self.num_blocks, self.num_samples),
        ).astype(self.np_dtype)
        if self.is_interleave:
            in0 = in0.T
        return in0

    def exec(self, x):
        """Kernel reference compute."""
        return np.ceil(x)
