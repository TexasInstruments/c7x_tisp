# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

"""
Every column of the input (dimY rows x dimX columns) is written as a row of the output (dimX x dimY).
TISP MatTrans supports float and double.
"""

import numpy as np

_SUPPORTED = ("float32", "float64")


class MatTrans:
    """Reference model of the DSPLIB matrix transpose kernel."""

    def __init__(self, *, rows, cols, in_dtype, num_blocks=1, **_):
        """
        Args:
            rows (int): number of rows of the input matrix
            cols (int): number of columns of the input matrix
            in_dtype (str): numpy dtype name, "float32" or "float64"
            num_blocks (int): independent matrices run back to back. generate_inputs() returns them stacked
                along the row axis ([num_blocks * rows, cols]); exec() takes one block
        """
        self.rows = int(rows)
        self.cols = int(cols)
        self.in_dtype = in_dtype
        self.num_blocks = int(num_blocks)
        self.init()

    def init(self):
        """Validate the data type (called by the constructor)."""
        self.np_dtype = np.dtype(self.in_dtype)
        if self.np_dtype.name not in _SUPPORTED:
            raise ValueError(f"Unsupported data type: {self.in_dtype}")

    def exec(self, x):
        """Transpose one block: a [rows, cols] matrix into [cols, rows]."""
        x = np.asarray(x)
        if x.shape != (self.rows, self.cols):
            raise ValueError(
                f"Input matrix shape {x.shape} does not match "
                f"expected shape ({self.rows}, {self.cols})"
            )
        return np.ascontiguousarray(x.astype(self.np_dtype).T)

    def generate_inputs(self, min_val=-1.0, max_val=1.0):
        """SDK test signal: random [num_blocks * rows, cols] matrices (for gen_data.py only)."""
        return np.random.uniform(
            low=min_val, high=max_val, size=(self.num_blocks * self.rows, self.cols)
        ).astype(self.np_dtype)
