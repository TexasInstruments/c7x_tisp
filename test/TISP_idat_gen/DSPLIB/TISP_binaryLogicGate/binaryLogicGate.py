# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np



class BinaryLogicGate:
    """Reference model of the binaryLogicGate kernel (opType 0: AND, 1: OR, otherwise XOR)."""

    def __init__(self, *, dType, opType, **_):
        self.dType = dType
        self.opType = opType
        self.init()

    def init(self):
        """Resolve the numpy type and the value range (called by the constructor)."""
        self.npType = self.dType[:-2]
        # Full range of the type; uint64_t is limited to 63 bits so every value is a valid C integer literal
        self.maxVal = min(int(np.iinfo(self.npType).max), 2**63 - 1)

    def generate_inputs(self, shape):
        """Generate the two test inputs of the given 2D shape (test-data only)."""
        in0 = np.random.randint(low=0, high=self.maxVal + 1, size=shape, dtype=np.uint64).astype(self.npType)
        in1 = np.random.randint(low=0, high=self.maxVal + 1, size=shape, dtype=np.uint64).astype(self.npType)
        return in0, in1

    def exec(self, in0, in1):
        """Element-wise bitwise gate."""
        if self.opType == 0:
            return in0 & in1
        elif self.opType == 1:
            return in0 | in1
        else:
            return in0 ^ in1
