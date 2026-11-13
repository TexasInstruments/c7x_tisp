# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class Router:
    """Reference model of the router kernel (input -> output channel routing).

    One ``exec`` call processes one frame: ``[num_samples, num_channels]`` when
    ``is_interleaved`` else ``[num_channels, num_samples]``; the output has
    ``num_out_channels`` channels.
    """

    def __init__(
        self,
        *,
        num_samples,
        num_channels,
        num_out_channels=None,
        in_dtype,
        is_interleaved,
        pChannelIndices,
        **_,
    ):
        self.num_samples = int(num_samples)
        self.num_channels = int(num_channels)
        self.is_interleaved = bool(is_interleaved)
        self.dtype = np.dtype(in_dtype)
        self.indices = [int(c) for c in np.asarray(pChannelIndices).reshape(-1)]
        self.num_out_channels = len(self.indices)
        if num_out_channels is not None and int(num_out_channels) != self.num_out_channels:
            raise ValueError("num_out_channels does not match len(pChannelIndices)")

    def exec(self, x):
        """Route input channels to output channels for one frame.

        An out-of-range channel index in pChannelIndices produces a zero channel.
        """
        output = []
        if self.is_interleaved:
            for ch in self.indices:
                if 0 <= ch < self.num_channels:  # valid channel index
                    output.append(x[:, ch])
                else:  # invalid -> pad with zeros
                    output.append(np.zeros(x.shape[0], dtype=x.dtype))
            return np.stack(output, axis=1)

        for ch in self.indices:
            if 0 <= ch < self.num_channels:  # valid channel index
                output.append(x[ch, :])
            else:  # invalid -> pad with zeros
                output.append(np.zeros(x.shape[1], dtype=x.dtype))
        return np.vstack(output)

    def generate_inputs(self, num_blocks=1, min_val=-10.0, max_val=10.0):
        """SDK test signal (for gen_data.py only).

        Shape is (samples, channels * num_blocks) when interleaved, otherwise
        (channels * num_blocks, samples).
        """
        if self.is_interleaved:
            size = (self.num_samples, self.num_channels * num_blocks)
        else:
            size = (self.num_channels * num_blocks, self.num_samples)
        return np.random.uniform(low=min_val, high=max_val, size=size).astype(self.dtype)
