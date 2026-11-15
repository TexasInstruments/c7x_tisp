# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class Round:
    """Reference model of the round kernel (round to nearest integer, halfway cases to even).

    One ``exec`` call rounds one frame of ``rows x cols`` samples. Halfway cases go to
    the even neighbour and the sign of the input is kept (-0.3 -> -0.0), as std::rint
    does in the kernel.
    """

    def __init__(self, *, rows, cols, in_dtype, **_):
        """
        Args:
            rows (int): Rows per frame (channels, or samples when interleaved).
            cols (int): Columns per frame.
            in_dtype: Sample dtype, "float32"/"float64" (or "float"/"double").
        """
        self.rows = int(rows)
        self.cols = int(cols)
        self.dtype = np.dtype({"float": "float32", "double": "float64"}.get(in_dtype, in_dtype))

    def exec(self, in0):
        """Round one frame as the kernel does."""
        return np.rint(in0)

    def generate_inputs(self, num_blocks=1):
        """SDK test signal (for gen_data.py only).

        Returns an input buffer of shape (rows * num_blocks, cols).
        """
        [minVal, maxVal] = [-10, 10]
        return np.random.uniform(
            low=minVal, high=maxVal, size=(self.rows * num_blocks, self.cols)
        ).astype(self.dtype)
