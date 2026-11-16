# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np



class ConditionalReplace:
    """Reference model of the conditionalReplace kernel (per-channel compare against a constant, replace on hit)."""

    def __init__(self, *, dType, numChannels, testId, **_):
        self.dType = dType
        self.numChannels = int(numChannels)
        self.testId = int(testId)
        self.init()

    def init(self):
        """Resolve the numpy type and the per-channel compare types (called by the constructor)."""
        self.npType = self.dType[:-2]
        # one entry per channel; every compare type occurs when there are at least 6 channels
        self.compareType = ((np.arange(self.numChannels) + self.testId) % 6).astype(self.npType)

    def generate_inputs(self, shape):
        """Generate the input of the given 2D shape plus the per-channel parameters (test-data only).

        Returns (in0, in1) where in1 = concat(compareType, constValue, replaceValue), one entry per channel each.
        """
        npType = self.npType
        # [-5, 5]: small enough that samples often equal the constants, so == and != see both outcomes
        in0 = np.random.randint(low=-5, high=6, size=shape).astype(npType)
        self.constValue = np.random.randint(low=-3, high=4, size=self.numChannels).astype(npType)
        self.replaceValue = np.random.randint(low=-100, high=101, size=self.numChannels).astype(npType)
        in1 = np.concatenate([self.compareType, self.constValue, self.replaceValue])
        return in0, in1

    def exec(self, x):
        """Replace x with replaceValue[ch] where the compare against constValue[ch] hits; row r belongs to channel r % numChannels."""
        out = np.zeros_like(x)
        for row in range(x.shape[0]):
            ch = row % self.numChannels
            xr = x[row]
            c = self.constValue[ch]
            hit = [xr == c, xr != c, xr < c, xr <= c, xr > c, xr >= c][int(self.compareType[ch])]
            out[row] = np.where(hit, self.replaceValue[ch], xr)
        return out
