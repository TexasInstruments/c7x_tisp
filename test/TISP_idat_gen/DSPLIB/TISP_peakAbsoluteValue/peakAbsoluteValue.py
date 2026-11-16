# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class PeakAbsoluteValue:
    """Reference model of the peakAbsoluteValue kernel (maximum absolute value per row)."""

    def __init__(self, *, dtype, num_rows, num_cols, num_blocks=1, **_):
        self.dtype = dtype
        self.num_rows = int(num_rows)  # rows of one block
        self.num_cols = int(num_cols)
        self.num_blocks = int(num_blocks)
        self.init()

    def init(self):
        dType = self.dtype
        self.shape = (self.num_blocks, self.num_rows, self.num_cols)
        if dType[0] == "f" or dType[0] == "d":
            self.np_type = "float32" if dType[0] == "f" else "float64"
            self.min_val, self.max_val = -10, 10
        else:
            self.np_type = dType[:-2]
            # [-1000, 1000] clipped to the type, without the type minimum: its absolute value does not fit the type
            self.min_val = max(np.iinfo(self.np_type).min + 1, -1000)
            self.max_val = min(np.iinfo(self.np_type).max, 1000)

    def generate_inputs(self):
        if self.dtype[0] == "f" or self.dtype[0] == "d":
            in0 = np.random.uniform(low=self.min_val, high=self.max_val, size=self.shape).astype(self.np_type)
        else:
            in0 = np.random.randint(low=self.min_val, high=self.max_val + 1, size=self.shape).astype(self.np_type)
        return in0.reshape(self.num_blocks * self.num_rows, self.num_cols)

    def exec(self, in0):
        """One maximum absolute value per row of every block, shape (num_blocks, num_rows)."""
        in0 = np.asarray(in0).reshape(self.shape)
        return np.max(np.abs(in0), axis=2).astype(self.np_type)
