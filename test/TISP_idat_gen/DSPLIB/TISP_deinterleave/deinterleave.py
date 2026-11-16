# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class Deinterleave:
    """Reference model of the deinterleave kernel (per-block 2D transpose)."""

    def __init__(self, *, dtype, width_in, height_in, num_blocks=1, **_):
        self.dtype = dtype  # C type name, e.g. "int16_t", "float"
        self.width_in = int(width_in)
        self.height_in = int(height_in)
        self.num_blocks = int(num_blocks)
        self.init()

    def init(self):
        """Precompute the input value range for the data type."""
        dType = self.dtype
        if dType in ("int8_t", "uint8_t"):
            bits = 8
        elif dType in ("int16_t", "uint16_t"):
            bits = 16
        else:
            bits = 32
        if dType[0] == "u":
            self.min_val = 0
            self.max_val = (2**bits) - 1
        else:
            self.min_val = -(2 ** (bits - 1))
            self.max_val = 2 ** (bits - 1)
        if dType == "float":
            self.np_type = "float"
            self.min_val, self.max_val = -10, 10
        elif dType == "double":
            self.np_type = "double"
            self.min_val, self.max_val = -10, 10
        else:
            self.np_type = dType[:-2]  # int8_t -> int8, uint64_t -> uint64

    def generate_inputs(self):
        """Random input of shape (height_in * num_blocks, width_in)."""
        return np.random.uniform(
            low=self.min_val,
            high=self.max_val,
            size=(self.height_in * self.num_blocks, self.width_in),
        ).astype(self.np_type)

    def exec(self, inp):
        """Transpose every block of height_in rows; blocks are stacked vertically."""
        transposed_blocks = []
        for i in range(self.num_blocks):
            block = inp[i * self.height_in : (i + 1) * self.height_in, :]
            transposed_blocks.append(block.T)
        return np.vstack(transposed_blocks)
