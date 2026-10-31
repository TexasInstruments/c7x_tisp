# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: Apache-2.0

import numpy as np


def undb20(x):
    return 10.0 ** (x / 20.0)


class AUDIOLIB_agc:

    def __init__(
        self,
        inChannels=1,
        inSamples=32,
        samplingRate=48000.0,
        targetLevel=-20.0,
        maxAttenuation=100.0,
        maxGain=12.0,
        ratio=10.0,
        activationThreshold=-50.0,
        smoothingTime=100.0,
        enableRecovery=1,
        recoveryRate=2.4,
        currentGain=0.0,
    ):
        self.inChannels = inChannels
        self.inSamples = inSamples
        self.samplingRate = float(samplingRate)

        self.targetLevel = float(targetLevel)
        self.maxAttenuation = float(maxAttenuation)
        self.maxGain = float(maxGain)
        self.ratio = float(ratio)
        self.activationThreshold = float(activationThreshold)
        self.smoothingTime = float(smoothingTime)
        self.enableRecovery = int(enableRecovery)
        self.recoveryRate = float(recoveryRate)
        self.currentGain = float(currentGain)

        blockTime = inSamples / self.samplingRate
        smoothingTimeSec = self.smoothingTime / 1000.0
        self.oneOverSlope = 1.0 - 1.0 / self.ratio
        self.smoothingCoeffSample = 1.0 - np.exp(-1.0 / (self.samplingRate * smoothingTimeSec))
        self.smoothingCoeffBlock = 1.0 - np.exp(-blockTime / smoothingTimeSec)
        self.recoveryRateUp = undb20(self.recoveryRate * blockTime)
        self.recoveryRateDown = undb20(-self.recoveryRate * blockTime)
        self.oneOverNumSamples = 1.0 / (inSamples * inChannels)

        self.targetGain = 1.0
        self.energy = 0.0

    def derived_params(self):
        return {
            "oneOverSlope": self.oneOverSlope,
            "smoothingCoeffSample": self.smoothingCoeffSample,
            "smoothingCoeffBlock": self.smoothingCoeffBlock,
            "recoveryRateUp": self.recoveryRateUp,
            "recoveryRateDown": self.recoveryRateDown,
            "oneOverNumSamples": self.oneOverNumSamples,
            "targetGain": self.targetGain,
            "energy": self.energy,
        }

    def exec(self, in0):
        """Process one block.

        Args:
            in0 (numpy.ndarray): input of shape (inChannels, inSamples)

        Returns:
            numpy.ndarray: output of shape (inChannels, inSamples)
        """
        if in0.shape != (self.inChannels, self.inSamples):
            raise ValueError(
                f"Expected block shape {(self.inChannels, self.inSamples)}, got {in0.shape}"
            )

        x = in0.astype(np.float64)
        blockEnergy = np.sum(x * x) * self.oneOverNumSamples
        self.energy = (
            self.energy * (1.0 - self.smoothingCoeffBlock)
            + blockEnergy * self.smoothingCoeffBlock
        )

        level = 10.0 * np.log10(self.energy) if self.energy > 0.0 else -np.inf

        if level >= self.activationThreshold:
            gainDB = (self.targetLevel - level) * self.oneOverSlope
            gainDB = min(max(gainDB, -self.maxAttenuation), self.maxGain)
            self.targetGain = undb20(gainDB)
        elif self.enableRecovery:
            # Below threshold: slew the held gain back towards 0 dB
            if self.targetGain > 1.0:
                self.targetGain = max(self.targetGain * self.recoveryRateDown, 1.0)
            elif self.targetGain < 1.0:
                self.targetGain = min(self.targetGain * self.recoveryRateUp, 1.0)

        decay = 1.0 - self.smoothingCoeffSample
        out = np.empty((self.inChannels, self.inSamples), dtype=np.float64)
        g = self.currentGain
        for m in range(self.inChannels):
            for n in range(self.inSamples):
                g = self.targetGain + decay * (g - self.targetGain)
                out[m, n] = x[m, n] * g
        self.currentGain = g

        return out
