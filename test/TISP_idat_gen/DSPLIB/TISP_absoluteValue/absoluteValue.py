# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np



class AbsoluteValue:
    """Reference model of the absoluteValue kernel (element-wise |x|)."""

    def __init__(self, *, dType, **_):
        self.dType = dType
        self.init()

    def init(self):
        """Resolve the numpy type from the C type name (called by the constructor)."""
        d = self.dType
        if d[0] == "f" or d[0] == "d":
            self.npType = "float32" if d[0] == "f" else "float64"
        else:
            self.npType = d[:-2]

    def generate_inputs(self, shape):
        """Generate the test input of the given 2D shape (test-data only)."""
        npType = self.npType
        if self.dType[0] == "f" or self.dType[0] == "d":
            [minVal, maxVal] = [-10, 10]
            in0 = np.random.uniform(low=minVal, high=maxVal, size=shape).astype(npType)
        else:
            # [-1000, 1000] clipped to the type; the most negative value is excluded because the optimized
            # kernel saturates abs(min) to max while the natural C kernel wraps it back to min
            [minVal, maxVal] = [max(np.iinfo(npType).min + 1, -1000), min(np.iinfo(npType).max, 1000)]
            in0 = np.random.randint(low=minVal, high=maxVal + 1, size=shape).astype(npType)
        return in0

    def exec(self, x):
        """Element-wise absolute value."""
        return np.abs(x)
