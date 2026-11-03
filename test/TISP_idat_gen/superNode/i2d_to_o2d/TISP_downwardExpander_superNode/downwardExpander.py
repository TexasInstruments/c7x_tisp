# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: Apache-2.0

import numpy as np


class AUDIOLIB_downwardExpander:
    """Downward expander with soft knee, one-pole attack/release smoothing and a hold counter.

    Per channel and per sample:

        x_dB = max(20 * log10(|x|), -313.0712)           (MATLAB floors the level at 20*log10(eps))
        target gain g_c in dB, from the static curve (T = threshold, R = ratio, W = knee width):
            x <  T - W/2        g_c = (x - T) * (R - 1)
            T - W/2 <= x <= T + W/2   g_c = (1 - R) * (x - T - W/2)^2 / (2 W)
            x >  T + W/2        g_c = 0
        gain smoothing g_s (dB):
            g_c > g_s   release: counter = 0, g_s = releaseCoeff * g_s + (1 - releaseCoeff) * g_c - FLT_MIN
                        (the -FLT_MIN floor applies only when releaseCoeff > 0)
            g_c <= g_s  attack: counter += 1 only if g_c < g_s (unchanged at g_c == g_s);
                        once counter > holdSamples, g_s = attackCoeff * g_s + (1 - attackCoeff) * g_c
        y = x * 10^(g_s / 20)

    The kernel API is modelled as set() (called whenever the user changes the parameters) followed
    by exec() on every block. The envelope and the hold counter start from rest and carry over
    between exec() calls. Subnormal results are flushed to zero, as on the C7x.
    """

    LEVEL_FLOOR_DB = -313.0712
    FLT_MIN = 1.17549435e-38
    FLT_MAX = 3.4028235e38

    def __init__(self, dType="float", inChannels=1, inSamples=1):
        """Initialize the AUDIOLIB_downwardExpander class with private members.

        Args:
            dType (str): Data type, e.g., "float"
            inChannels (int): Number of input channels
            inSamples (int): Number of samples per channel per frame
        """
        self._dType = dType.lower()
        self._npDtype = np.float64 if self._dType == "double" else np.float32
        self._inChannels = int(inChannels)
        self._inSamples = int(inSamples)

        # Internal state tracking per channel across frames
        self._smoothedGain = np.zeros(self._inChannels, dtype=np.float64)
        self._holdCount = np.zeros(self._inChannels, dtype=np.float64)

        self._thresholdDb = np.zeros(self._inChannels)
        self._ratio = np.ones(self._inChannels)
        self._kneeWidthDb = np.zeros(self._inChannels)
        self._attackCoeff = np.zeros(self._inChannels)
        self._releaseCoeff = np.zeros(self._inChannels)
        self._holdSamples = np.zeros(self._inChannels)

    def set(self, thresholdDb, ratio, kneeWidthDb, attackCoeff, releaseCoeff, holdSamples):
        """Set the per-channel parameters (arrays of length inChannels)."""
        self._thresholdDb = np.asarray(thresholdDb, dtype=np.float64)
        self._ratio = np.asarray(ratio, dtype=np.float64)
        self._kneeWidthDb = np.asarray(kneeWidthDb, dtype=np.float64)
        self._attackCoeff = np.asarray(attackCoeff, dtype=np.float64)
        self._releaseCoeff = np.asarray(releaseCoeff, dtype=np.float64)
        self._holdSamples = np.asarray(holdSamples, dtype=np.float64)

    def exec(self, x):
        """Process one block.

        Args:
            x (ndarray): input, shape (inChannels, inSamples)

        Returns:
            ndarray: output, shape (inChannels, inSamples)
        """
        x = np.asarray(x, dtype=np.float64)
        y = np.zeros_like(x)

        for ch in range(self._inChannels):
            threshold = self._thresholdDb[ch]
            ratio = self._ratio[ch]
            kneeWidth = self._kneeWidthDb[ch]
            attackCoeff = self._attackCoeff[ch]
            releaseCoeff = self._releaseCoeff[ch]
            holdSamples = self._holdSamples[ch]

            kneeStart = threshold - kneeWidth / 2
            kneeEnd = threshold + kneeWidth / 2

            smoothedGain = self._smoothedGain[ch]
            holdCount = self._holdCount[ch]

            for n in range(self._inSamples):
                with np.errstate(divide="ignore"):
                    level = max(20 * np.log10(abs(x[ch, n])), self.LEVEL_FLOOR_DB)

                if kneeWidth <= 0:
                    targetGain = (level - threshold) * (ratio - 1) if level < threshold else 0.0
                elif level < kneeStart:
                    targetGain = (level - threshold) * (ratio - 1)
                elif level > kneeEnd:
                    targetGain = 0.0
                else:
                    targetGain = (1 - ratio) * (level - kneeEnd) ** 2 / (2 * kneeWidth)

                if targetGain > smoothedGain:
                    holdCount = 0
                    # A release toward 0 dB stops at -FLT_MIN instead of reaching 0 (the C7x flushes
                    # subnormals), so the next 0 dB target resets the hold counter, as in MATLAB.
                    releaseFloor = -self.FLT_MIN if releaseCoeff > 0 else 0.0
                    smoothedGain = releaseCoeff * smoothedGain + ((1 - releaseCoeff) * targetGain + releaseFloor)
                else:
                    if targetGain < smoothedGain:
                        holdCount += 1
                    if holdCount > holdSamples:
                        smoothedGain = attackCoeff * smoothedGain + (1 - attackCoeff) * targetGain

                linearGain = min(10 ** (smoothedGain / 20), self.FLT_MAX)
                if linearGain < self.FLT_MIN:
                    linearGain = 0.0

                y[ch, n] = x[ch, n] * linearGain

            self._smoothedGain[ch] = smoothedGain
            self._holdCount[ch] = holdCount

        return y
