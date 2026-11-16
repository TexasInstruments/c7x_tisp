# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class DeEmphasis:
    """Reference model of the deEmphasis kernel: y[n] = x[n] + alpha * y[n-1], per channel along the samples.

    The filter memory starts from silence and is carried across the blocks (exec calls).
    """

    def __init__(self, *, alpha, num_channels, in_dtype="float32", **_):
        self.alpha = np.float32(alpha)
        self.num_channels = int(num_channels)
        self.dtype = np.dtype(in_dtype)
        self.init()

    def init(self):
        # Filter memory (one value per channel), starting from silence
        self.prev = np.zeros(self.num_channels, dtype=np.float32)

    def generate_inputs(self, *, shape, min_val=-10, max_val=10):
        """Uniform test data of the given shape, in the sample data type."""
        return np.random.uniform(low=min_val, high=max_val, size=shape).astype(
            self.dtype
        )

    def exec(self, x):
        """x: one block [num_channels, num_samples]; returns the filtered block and
        advances the filter memory."""
        x = np.asarray(x)
        y = np.empty_like(x)
        for n in range(x.shape[1]):
            self.prev = x[:, n] + self.alpha * self.prev
            y[:, n] = self.prev
        return y
