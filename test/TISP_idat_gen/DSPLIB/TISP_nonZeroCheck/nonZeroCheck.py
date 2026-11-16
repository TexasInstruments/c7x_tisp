# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class NonZeroCheck:
    """Reference model of the nonZeroCheck kernel (ALL/ANY non-zero reduction per row/column)."""

    def __init__(self, *, dtype, op_type, axis_type, num_rows, num_cols, num_blocks=1, **_):
        self.dtype = dtype
        self.op_type = int(op_type)  # 0: ALL non-zero, else ANY non-zero
        self.axis_type = int(axis_type)  # 0: ROW (reduce over rows), else COLUMN (reduce over columns)
        self.num_rows = int(num_rows)  # rows of one block
        self.num_cols = int(num_cols)
        self.num_blocks = int(num_blocks)
        self.init()

    def init(self):
        self.np_type = {"float": "float32", "double": "float64"}.get(self.dtype, self.dtype[:-2])
        self.shape = (self.num_blocks, self.num_rows, self.num_cols)
        # ALL needs mostly non-zero data and ANY mostly zero data, so that both results occur
        self.prob_non_zero = 0.98 if self.op_type == 0 else 0.02

    def generate_inputs(self):
        shape = self.shape
        dType = self.dtype
        nonZero = np.random.uniform(size=shape) < self.prob_non_zero
        if dType[0] == "f" or dType[0] == "d":
            values = np.random.uniform(low=1, high=2, size=shape) * np.random.choice([-1, 1], size=shape)
            zeros = np.random.choice([0.0, -0.0], size=shape)
        else:
            values = np.random.randint(low=1, high=4, size=shape) * np.random.choice(
                [-1, 1] if dType[0] == "i" else [1], size=shape
            )
            zeros = np.zeros(shape)
        in0 = np.where(nonZero, values, zeros).astype(self.np_type)
        return in0.reshape(self.num_blocks * self.num_rows, self.num_cols)

    def exec(self, in0):
        """Reduce every block: ROW axis over the rows (one result per column), COLUMN over the columns."""
        in0 = np.asarray(in0).reshape(self.shape)
        reduceAxis = 1 if self.axis_type == 0 else 2
        if self.op_type == 0:
            out = np.all(in0 != 0, axis=reduceAxis)
        else:
            out = np.any(in0 != 0, axis=reduceAxis)
        return out.astype(self.np_type).reshape(-1)
