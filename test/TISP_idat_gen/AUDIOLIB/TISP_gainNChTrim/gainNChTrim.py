# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class GainNChTrim:
    """Reference model of the gainNChTrim kernel (per-channel gain + master gain).

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
        pGains,
        pMasterGain,
        bypassMode=0,
        **_,
    ):
        self.num_samples = int(num_samples)
        self.num_channels = int(num_channels)
        self.is_interleaved = bool(is_interleaved)
        self.dtype = np.dtype(in_dtype)
        self.gains = np.asarray(pGains, dtype=self.dtype).reshape(-1)
        self.master_gain = np.asarray(pMasterGain, dtype=self.dtype).reshape(-1)[0]
        self.bypass = bool(bypassMode)

    def exec(self, x):
        """Apply per-channel gain and master gain to one frame."""
        # bypass copies the input to the output, leaving the gain unapplied
        if self.bypass:
            return x.copy()
        if self.is_interleaved:
            return x * self.gains * self.master_gain
        return x * self.gains[:, np.newaxis] * self.master_gain

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

        A classmethod because the gains are drawn after the input (to keep the
        random stream, hence the generated vectors, unchanged) and are needed to
        build the model.

        Returns:
            (x, pGains, pMasterGain): x has shape (channels * num_blocks, samples),
            transposed when interleaved; pGains has shape (channels,); pMasterGain
            is a one-element float32 array.
        """
        dtype = np.dtype(in_dtype)
        x = np.random.uniform(
            low=min_val, high=max_val, size=(num_channels * num_blocks, num_samples)
        ).astype(dtype)
        gains = np.random.uniform(low=min_val, high=max_val, size=(num_channels)).astype(dtype)
        master = np.random.uniform(low=min_val, high=max_val, size=(1,)).astype("float32")
        if is_interleaved:
            x = x.T
        return x, gains, master
