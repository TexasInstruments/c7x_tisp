# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class ThresholdDetector:
    """Reference model of the thresholdDetector kernel (compare every element with a constant)."""

    def __init__(self, *, dtype, op_type, constant, num_rows, num_cols, is_interleave=0, **_):
        self.dtype = dtype
        self.op_type = int(op_type)  # 0 ==, 1 !=, 2 <, 3 <=, 4 >, else >=
        self.constant_value = constant
        self.num_rows = int(num_rows)  # in_channels * num_blocks
        self.num_cols = int(num_cols)
        self.is_interleave = bool(is_interleave)
        self.init()

    def init(self):
        dType = self.dtype
        if dType[0] == "f" or dType[0] == "d":
            self.np_type = "float32" if dType[0] == "f" else "float64"
            self.min_val, self.max_val = -10, 10
        else:
            self.np_type = dType[:-2]
            # Full range for 8-bit types, [-1000, 1000] clipped to the type otherwise
            self.min_val = max(np.iinfo(self.np_type).min, -1000)
            self.max_val = min(np.iinfo(self.np_type).max, 1000)
        self.constant = np.array(float(self.constant_value)).astype(self.np_type)

    def generate_inputs(self):
        size = (self.num_rows, self.num_cols)
        if self.dtype[0] == "f" or self.dtype[0] == "d":
            in0 = np.random.uniform(low=self.min_val, high=self.max_val, size=size).astype(self.np_type)
        else:
            in0 = np.random.randint(low=self.min_val, high=self.max_val + 1, size=size).astype(self.np_type)
        # Set about a quarter of the elements to the constant so ==, <= and >= see equal values
        in0[np.random.uniform(size=in0.shape) < 0.25] = self.constant
        if self.is_interleave:
            in0 = in0.T
        return in0

    def exec(self, in0):
        """Boolean comparison result (the caller casts it to the unsigned output type)."""
        c = self.constant
        if self.op_type == 0:
            return in0 == c
        elif self.op_type == 1:
            return in0 != c
        elif self.op_type == 2:
            return in0 < c
        elif self.op_type == 3:
            return in0 <= c
        elif self.op_type == 4:
            return in0 > c
        return in0 >= c
