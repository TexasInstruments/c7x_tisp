# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np



class BitwiseANDConst:
    """Reference model of the bitwiseANDConst kernel (x & constant)."""

    def __init__(self, *, dType, constant, **_):
        self.dType = dType
        self.constant = constant
        self.init()

    def init(self):
        """Resolve the numpy type and the constant mask (called by the constructor)."""
        self.npType = self.dType[:-2]
        self.mask = np.array(self.constant).astype(self.npType)

    def generate_inputs(self, shape):
        """Generate the test input of the given 2D shape (test-data only)."""
        # the whole range of the type, so that every bit of the mask is exercised
        return np.random.randint(
            low=0, high=int(np.iinfo(self.npType).max) + 1, size=shape, dtype=self.npType
        )

    def exec(self, x):
        """Bitwise AND with the constant."""
        return np.bitwise_and(x, self.mask)
