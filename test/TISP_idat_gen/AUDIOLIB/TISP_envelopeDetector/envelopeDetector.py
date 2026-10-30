# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: Apache-2.0

import numpy as np


class AUDIOLIB_envelopeDetector:

    def __init__(
        self,
        inChannels=1,
        inSamples=32,
        samplingRate=48000.0,
        attackTime=0.010,
        releaseTime=0.100,
    ):
        self.inChannels = inChannels
        self.inSamples = inSamples
        self.samplingRate = float(samplingRate)

        self.attackTime = float(attackTime)
        self.releaseTime = float(releaseTime)

        fs = float(samplingRate)
        self.attackCoeff = 1.0 - np.exp(-1.0 / (fs * self.attackTime))
        self.releaseCoeff = 1.0 - np.exp(-1.0 / (fs * self.releaseTime))

        self.envStates = np.zeros(inChannels, dtype=np.float32)

    def derived_params(self):
        return {
            "attackCoeff": self.attackCoeff,
            "releaseCoeff": self.releaseCoeff,
            "envStates": self.envStates.tolist(),
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
        out = np.empty((self.inChannels, self.inSamples), dtype=np.float64)

        for m in range(self.inChannels):
            env = float(self.envStates[m])
            for n in range(self.inSamples):
                inputAbs = abs(x[m, n])
                coeff = self.attackCoeff if inputAbs > env else self.releaseCoeff
                env = (1.0 - coeff) * env + coeff * inputAbs
                out[m, n] = env
            self.envStates[m] = env

        return out
