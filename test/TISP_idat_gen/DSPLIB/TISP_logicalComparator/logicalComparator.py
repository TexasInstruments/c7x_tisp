# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np



class LogicalComparator:
    """Reference model of the logicalComparator kernel (opType 0:==, 1:!=, 2:<, 3:<=, 4:>, otherwise >=)."""

    OUT_TYPE = {
        "int8_t": "uint8",
        "uint8_t": "uint8",
        "int16_t": "uint16",
        "uint16_t": "uint16",
        "int32_t": "uint32",
        "uint32_t": "uint32",
        "int64_t": "uint64",
        "uint64_t": "uint64",
        "float": "uint32",
        "double": "uint64",
    }

    def __init__(self, *, dType, opType, **_):
        self.dType = dType
        self.opType = opType
        self.init()

    def init(self):
        """Resolve the input and output numpy types (called by the constructor)."""
        d = self.dType
        self.isFloat = d[0] == "f" or d[0] == "d"
        self.npType = ("float32" if d[0] == "f" else "float64") if self.isFloat else d[:-2]
        self.outNpType = self.OUT_TYPE[d]

    def generate_inputs(self, shape):
        """Generate the two test inputs of the given 2D shape (test-data only)."""
        npType = self.npType
        if self.isFloat:
            [minVal, maxVal] = [-10, 10]
            in0 = np.random.uniform(low=minVal, high=maxVal, size=shape).astype(npType)
            in1 = np.random.uniform(low=minVal, high=maxVal, size=shape).astype(npType)
        else:
            # Full range for 8-bit types, [-1000, 1000] clipped to the type otherwise
            [minVal, maxVal] = [max(np.iinfo(npType).min, -1000), min(np.iinfo(npType).max, 1000)]
            in0 = np.random.randint(low=minVal, high=maxVal + 1, size=shape).astype(npType)
            in1 = np.random.randint(low=minVal, high=maxVal + 1, size=shape).astype(npType)

        # Copy about a quarter of the elements of input 0 into input 1 so ==, <= and >= see equal values
        equal = np.random.uniform(size=in0.shape) < 0.25
        in1[equal] = in0[equal]
        return in0, in1

    def exec(self, in0, in1):
        """Element-wise compare; result is the unsigned integer type of the input's size."""
        if self.opType == 0:
            out = in0 == in1
        elif self.opType == 1:
            out = in0 != in1
        elif self.opType == 2:
            out = in0 < in1
        elif self.opType == 3:
            out = in0 <= in1
        elif self.opType == 4:
            out = in0 > in1
        else:
            out = in0 >= in1
        return out.astype(self.outNpType)
