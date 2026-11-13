import numpy as np


class Split:
    """Reference model of the split kernel: one input, N outputs of the given channel counts."""

    _kMaxOutputs = 64

    def __init__(self, *, num_samples, num_channels, in_dtype, is_interleaved,
                 num_outputs, out_channels_list=None, **_):
        self.num_samples = int(num_samples)
        self.num_channels = int(num_channels)
        self.in_dtype = in_dtype
        self.is_interleaved = is_interleaved
        self.num_outputs = int(num_outputs)
        self.out_channels_list = out_channels_list
        self.init()

    def init(self):
        """Resolve the channel counts and dtype (called by the constructor)."""
        self.np_dtype = np.dtype(self.in_dtype)
        self.is_interleaved = bool(self.is_interleaved)

        if self.out_channels_list is None or len(self.out_channels_list) == 0:
            self.out_channels_list = [self.num_channels // self.num_outputs] * self.num_outputs
        else:
            self.out_channels_list = [int(c) for c in self.out_channels_list]
        self.num_outputs = len(self.out_channels_list)

        # Validate numOutputs per spec (max 64)
        if not (1 <= self.num_outputs <= self._kMaxOutputs):
            raise ValueError(
                f"numOutputs must be between 1 and {self._kMaxOutputs}, got {self.num_outputs}"
            )
        if sum(self.out_channels_list) != self.num_channels:
            raise ValueError(
                f"out_channels_list {self.out_channels_list} does not sum to "
                f"num_channels {self.num_channels}"
            )

    def exec(self, x):
        """Split one frame.

        x: [num_samples, num_channels] (interleaved) or [num_channels, num_samples] (planar).
        Returns a list with one array per output, in the same layout as the input.
        """
        x = np.asarray(x)
        frame_in = x.reshape(
            (self.num_samples, self.num_channels) if self.is_interleaved
            else (self.num_channels, self.num_samples)
        )
        outs = []
        ch_start = 0
        for n_ch in self.out_channels_list:
            ch_end = ch_start + n_ch
            if self.is_interleaved:
                part = frame_in[:, ch_start:ch_end]
            else:
                part = frame_in[ch_start:ch_end, :]
            outs.append(np.ascontiguousarray(part).astype(self.np_dtype))
            ch_start = ch_end
        return outs
