# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: Apache-2.0


import math

import numpy as np


def rmsSmoothingCoefficient(blockSize, samplingRate, smoothingTime):
    """The kernel stores the coefficient as float for both data types."""
    if smoothingTime == 0.0:
        return np.float32(0.0)

    smoothingTimeSec = np.float32(smoothingTime) / np.float32(1000.0)
    denominator = np.float32(samplingRate) * smoothingTimeSec
    exponent = np.float32(-2.0 * math.pi * blockSize / float(denominator))
    return np.float32(math.exp(float(exponent)))


def rmsSmoothedProcess(src, blockSize, numChannels, samplingRate, smoothingTime, frames, dType=np.float32):
    """src: [frames, numChannels, blockSize], planar per frame.
    dType: np.float32 or np.float64, the precision of the computation. As in the kernel, the
    coefficient, 1 / numSamples and the carried smoothed energy are stored as float.
    Returns one smoothed RMS value per frame."""
    srcLocal = src.astype(dType).reshape(frames, numChannels, blockSize)
    smoothingCoefficient = dType(rmsSmoothingCoefficient(blockSize, samplingRate, smoothingTime))
    invNumSamples = dType(np.float32(1.0) / np.float32(blockSize * numChannels))

    smoothedEnergyState = np.float32(0.0)
    dst = np.empty(frames, dtype=dType)

    for frame in range(frames):
        energy = dType(0.0)
        for ch in range(numChannels):
            energy += np.sum(srcLocal[frame, ch] * srcLocal[frame, ch], dtype=dType)
        energy = energy * invNumSamples

        smoothedEnergy = dType(
            (smoothingCoefficient * dType(smoothedEnergyState))
            + ((dType(1.0) - smoothingCoefficient) * energy)
        )
        smoothedEnergyState = np.float32(smoothedEnergy)
        dst[frame] = np.sqrt(smoothedEnergy)

    return dst
