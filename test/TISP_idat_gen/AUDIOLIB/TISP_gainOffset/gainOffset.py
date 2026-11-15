# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class GainOffset:
    """Reference model of the gainOffset kernel (y = x * gain + offset).

    One ``exec`` call processes one frame: ``[num_samples, num_channels]`` when
    ``is_interleaved`` else ``[num_channels, num_samples]``.
    """

    def __init__(
        self,
        *,
        num_samples,
        num_channels,
        in_dtype,
        is_interleaved,
        gain,
        offset,
        **_,
    ):
        self.num_samples = int(num_samples)
        self.num_channels = int(num_channels)
        self.is_interleaved = bool(is_interleaved)
        self.dtype = np.dtype(in_dtype)
        self.is_int = self.dtype.kind == "i"
        if self.is_int:
            # int32 data: gain and offset are plain int32 parameters
            self.gain = np.int32(gain)
            self.offset = np.int32(offset)
        else:
            # float and double data: gain and offset are float32 parameters
            self.gain = np.float32(gain)
            self.offset = np.float32(offset)

    def exec(self, x):
        """Apply y = x * gain + offset to one frame."""
        if self.is_int:
            # int64 accumulate, saturated once to int32 (as the kernel does)
            acc = x.astype(np.int64) * np.int64(self.gain) + np.int64(self.offset)
            info = np.iinfo(self.dtype)
            return np.clip(acc, info.min, info.max).astype(self.dtype)
        # gain and offset in the input precision
        return x * self.dtype.type(self.gain) + self.dtype.type(self.offset)

    @classmethod
    def generate_inputs(
        cls,
        *,
        num_samples,
        num_channels,
        in_dtype,
        is_interleaved,
        num_blocks=1,
        min_val=-10.0,
        max_val=10.0,
        **_,
    ):
        """SDK test signal (for gen_data.py only).

        Returns:
            x: shape (channels * num_blocks, samples), transposed to
            (samples, channels * num_blocks) when interleaved.
        """
        dtype = np.dtype(in_dtype)
        size = (num_channels * num_blocks, num_samples)
        if dtype.kind == "i":
            x = np.random.randint(low=int(min_val), high=int(max_val) + 1, size=size)
        else:
            x = np.random.uniform(low=min_val, high=max_val, size=size)
        x = x.astype(dtype)
        if is_interleaved:
            x = x.T
        return x
