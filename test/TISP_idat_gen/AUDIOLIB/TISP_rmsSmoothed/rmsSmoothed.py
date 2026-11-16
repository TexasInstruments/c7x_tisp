# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import math

import numpy as np


class RmsSmoothed:
    """Reference model of the rmsSmoothed kernel (smoothed RMS over all channels, one value per block).

    As in the kernel, the coefficient, 1 / numSamples and the carried smoothed energy are
    stored as float, whatever the sample data type.
    """

    def __init__(
        self,
        *,
        block_size,
        num_channels,
        sampling_rate,
        smoothing_time,
        in_dtype="float32",
        **_,
    ):
        self.block_size = int(block_size)
        self.num_channels = int(num_channels)
        self.sampling_rate = sampling_rate
        self.smoothing_time = smoothing_time
        self.dtype = np.dtype(in_dtype)
        self.init()

    @staticmethod
    def smoothing_coefficient(block_size, sampling_rate, smoothing_time):
        """The kernel stores the coefficient as float for both data types."""
        if smoothing_time == 0.0:
            return np.float32(0.0)

        smoothingTimeSec = np.float32(smoothing_time) / np.float32(1000.0)
        denominator = np.float32(sampling_rate) * smoothingTimeSec
        exponent = np.float32(-2.0 * math.pi * block_size / float(denominator))
        return np.float32(math.exp(float(exponent)))

    def init(self):
        dType = self.dtype.type
        self.smoothingCoefficient = dType(
            self.smoothing_coefficient(
                self.block_size, self.sampling_rate, self.smoothing_time
            )
        )
        self.invNumSamples = dType(
            np.float32(1.0) / np.float32(self.block_size * self.num_channels)
        )
        # The smoothed energy starts from zero and carries across the blocks
        self.smoothedEnergyState = np.float32(0.0)

    def generate_inputs(self, *, shape, min_val=-10, max_val=10):
        """Uniform test data of the given shape, in the sample data type."""
        return np.random.uniform(low=min_val, high=max_val, size=shape).astype(
            self.dtype
        )

    def exec(self, x):
        """x: one block [num_channels, block_size]; returns the smoothed RMS of the block
        and advances the carried smoothed energy."""
        dType = self.dtype.type
        src = np.asarray(x).astype(dType).reshape(self.num_channels, self.block_size)
        energy = dType(0.0)
        for ch in range(self.num_channels):
            energy += np.sum(src[ch] * src[ch], dtype=dType)
        energy = energy * self.invNumSamples

        smoothedEnergy = dType(
            (self.smoothingCoefficient * dType(self.smoothedEnergyState))
            + ((dType(1.0) - self.smoothingCoefficient) * energy)
        )
        self.smoothedEnergyState = np.float32(smoothedEnergy)
        return np.sqrt(smoothedEnergy)
