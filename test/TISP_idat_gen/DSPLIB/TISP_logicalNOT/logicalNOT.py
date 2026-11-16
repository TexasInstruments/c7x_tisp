# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np



class LogicalNOT:
    """Reference model of the logicalNOT kernel (1 where x == 0, else 0, in the input type)."""

    def __init__(self, *, dType, **_):
        self.dType = dType
        self.init()

    def init(self):
        """Resolve the numpy type (called by the constructor)."""
        d = self.dType
        self.isFloat = d[0] == "f" or d[0] == "d"
        self.npType = ("float32" if d[0] == "f" else "float64") if self.isFloat else d[:-2]

    def generate_inputs(self, shape):
        """Generate the test input of the given 2D shape (test-data only)."""
        npType = self.npType
        if self.isFloat:
            # zeros of both signs are mixed with non-zero values so that both outputs (0 and 1) occur
            return np.random.choice([0.0, -0.0, 0.5, -1.5, 3.0], size=shape).astype(npType)
        # [-2, 2] clipped to the type, so that zeros occur often
        [minVal, maxVal] = [max(np.iinfo(npType).min, -2), min(np.iinfo(npType).max, 2)]
        return np.random.randint(low=minVal, high=maxVal + 1, size=shape).astype(npType)

    def exec(self, x):
        """Logical NOT: 1 where x == 0, else 0."""
        return (x == 0).astype(x.dtype)
