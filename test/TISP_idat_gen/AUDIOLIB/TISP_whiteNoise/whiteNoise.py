# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np

# Constants
a = 1664525
c = 1013904223
m = 2**32

# LCG steps between the start states of neighbouring channels (AUDIOLIB_WHITENOISEGENERATOR_CHANNEL_SPACING)
CHANNEL_SPACING = 51274189


def lcg_jump(x, steps):
    """Advance LCG state x by `steps` steps, using powers of the 2x2 matrix [[a, c], [0, 1]]."""
    result = [[1, 0], [0, 1]]
    base = [[a, c], [0, 1]]
    while steps > 0:
        if steps & 1:
            result = [
                [(result[0][0] * base[0][0]) % m, (result[0][0] * base[0][1] + result[0][1]) % m],
                [0, 1],
            ]
        base = [[(base[0][0] * base[0][0]) % m, (base[0][0] * base[0][1] + base[0][1]) % m], [0, 1]]
        steps >>= 1
    return (result[0][0] * x + result[0][1]) % m


class WhiteNoise:
    """Reference model of the whiteNoise kernel (multichannel LCG white noise).

    One ``exec`` call produces one frame of ``num_channels * num_samples`` samples
    (channel-major). Channel ch runs its own LCG sequence, starting
    ch * CHANNEL_SPACING steps after the seed; the per-channel LCG state is carried
    from frame to frame.
    """

    def __init__(
        self,
        *,
        num_samples,
        num_channels,
        seed,
        range_val,
        in_dtype,
        **_,
    ):
        """
        Args:
            num_samples (int): Number of samples per channel per frame.
            num_channels (int): Number of channels.
            seed (int): Initial LCG seed of channel 0.
            range_val (float): Output range [-range_val, range_val].
            in_dtype: Output dtype, "float32"/"float64" (or "float"/"double").
        """
        self.num_samples = int(num_samples)
        self.num_channels = int(num_channels)
        self.dtype = np.dtype({"float": "float32", "double": "float64"}.get(in_dtype, in_dtype))
        # (x - 2^31) / 2^31 * range, rounded as the kernel does: int32 -> float, then one multiply
        self.scale = self.dtype.type(self.dtype.type(range_val) * self.dtype.type(2.0**-31))
        self.reset(seed)

    def reset(self, seed):
        """(Re)start every channel's LCG sequence from the seed."""
        self.state = [lcg_jump(int(seed), ch * CHANNEL_SPACING) for ch in range(self.num_channels)]

    def exec(self):
        """Generate ONE frame and advance the LCG state.

        Returns:
            numpy.ndarray: (num_channels * num_samples) samples, channel-major.
        """
        x = self.state
        out = np.zeros(self.num_channels * self.num_samples, dtype=self.dtype)
        for ch in range(self.num_channels):
            for i in range(self.num_samples):
                x[ch] = (a * x[ch] + c) % m
                out[ch * self.num_samples + i] = self.dtype.type(x[ch] - 2**31) * self.scale
        return out
