# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


def norm32(a):
    """Redundant sign bits of a non-negative 32-bit value (C7x __norm)."""
    if a == 0:
        return 31
    return 31 - a.bit_length()


def sub_cond(b, a):
    """C7x __sub_cond on unsigned 32-bit operands."""
    if b >= a:
        return (((b - a) << 1) + 1) & 0xFFFFFFFF
    return (b << 1) & 0xFFFFFFFF


def recip_q15(x):
    """Fixed point reciprocal of a Q15 value, returns (exponent, fraction)."""
    a = int(x)
    neg = a < 0
    if neg:
        a = -a

    normal = norm32(a)
    a = (a << normal) & 0xFFFFFFFF
    exp = normal - 15

    b = 0x80000000
    for _ in range(15):
        b = sub_cond(b, a)

    frac = b & 0x7FFF
    if neg:
        frac = -frac

    return exp, frac


class Recip:
    """Reference model of the recip kernel (float reciprocal, or Q15 exponent/fraction pair)."""

    def __init__(self, *, dtype, num_rows, num_cols, store_style=0, num_blocks=1, block_width=None, **_):
        self.dtype = dtype
        self.num_rows = int(num_rows)  # rows of one block
        self.num_cols = int(num_cols)
        self.num_blocks = int(num_blocks)
        self.store_style = int(store_style)  # 1: HSTACK (exponents then fractions), else interleaved
        # columns per processed tile; None: the whole row is one tile
        self.block_width = self.num_cols if block_width is None else int(block_width)
        self.init()

    def init(self):
        self.np_type = {"float": "float32", "double": "float64", "int16_t": "int16"}[self.dtype]
        self.shape = (self.num_blocks, self.num_rows, self.num_cols)

    def generate_inputs(self):
        shape = self.shape
        npType = self.np_type
        if self.dtype[0] == "f" or self.dtype[0] == "d":
            # magnitude in [0.25, 16] with a random sign: no zero input, so no infinite reciprocal
            in0 = (np.random.uniform(low=0.25, high=16, size=shape) * np.random.choice([-1, 1], size=shape)).astype(npType)
        else:
            # Q15 magnitudes spread over all 15 bit lengths, no zero; the type minimum and maximum are always present
            in0 = (2 ** np.random.uniform(low=0, high=15, size=shape)).astype(np.int32)
            in0 = np.clip(in0, 1, 32767) * np.random.choice([-1, 1], size=shape)
            in0 = in0.astype(npType)
            in0[0, 0, 0] = -32768
            in0[-1, -1, -1] = 32767
        return in0.reshape(self.num_blocks * self.num_rows, self.num_cols)

    def exec(self, in0):
        shape = self.shape
        npType = self.np_type
        in0 = np.asarray(in0).reshape(shape)
        if self.dtype[0] == "f" or self.dtype[0] == "d":
            out = (np.array(1, dtype=npType) / in0).astype(npType)
        else:
            # Every tile of block_width columns is processed on its own and gives 2 * block_width values per row, so
            # the HSTACK layout (exponents then fractions) holds within a tile: | exp0..expN-1 | frac0..fracN-1 |
            bw = self.block_width
            out = np.zeros((shape[0], shape[1], 2 * shape[2]), dtype=npType)
            for n in range(shape[0]):
                for y in range(shape[1]):
                    for x in range(shape[2]):
                        exp, frac = recip_q15(in0[n, y, x])
                        base = 2 * bw * (x // bw)
                        xInTile = x % bw
                        if self.store_style == 1:
                            out[n, y, base + xInTile] = exp
                            out[n, y, base + bw + xInTile] = frac
                        else:
                            out[n, y, base + 2 * xInTile] = exp
                            out[n, y, base + 2 * xInTile + 1] = frac
        return out.reshape(-1)
