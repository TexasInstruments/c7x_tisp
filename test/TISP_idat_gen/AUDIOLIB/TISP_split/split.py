import numpy as np


class TISP_split:
    def __init__(self, dType="float", numSamples=1, outChannelsList=None,
                 numExecReps=1, isInputInterleave=1):

        if outChannelsList is None:
            outChannelsList = [1]
        self._dType             = dType
        self._numSamples        = numSamples
        self._outChannelsList   = list(outChannelsList)
        self._numOutputs        = len(self._outChannelsList)  # N - number of output ports
        self._totalChannels     = sum(self._outChannelsList)  # C_total
        self._numExecReps       = numExecReps
        self._isInputInterleave = isInputInterleave

        # Validate numOutputs per spec (max 64)
        self._kMaxOutputs = 64
        if not (1 <= self._numOutputs <= self._kMaxOutputs):
            raise ValueError(
                f"numOutputs must be between 1 and {self._kMaxOutputs}, got {self._numOutputs}"
            )

    def exec(self, pInLocal):

        N       = self._numOutputs     # Number of outputs
        S       = self._numSamples     # Samples per channel
        C_total = self._totalChannels  # Total input channels to split

        # Map dType string to numpy dtype
        dtype_map = {
            "float": np.float32,
            "double": np.float64,
            "int32": np.int32,
            "uint32": np.uint32,
            "int16": np.int16,
            "uint16": np.uint16,
            "int8": np.int8,
            "uint8": np.uint8,
        }
        np_dtype = dtype_map.get(self._dType, pInLocal.dtype)

        # Allocate N independent output arrays — one per output port
        pOutLocal = [
            np.zeros((self._numExecReps, S * self._outChannelsList[i]), dtype=np_dtype)
            for i in range(N)
        ]

        for frame in range(self._numExecReps):
            if self._isInputInterleave:
                # Reshape aggregated input for this frame to (samples, totalChannels)
                frame_in = pInLocal[frame].reshape((S, C_total))
                ch_start = 0
                for i in range(N):
                    ch_end = ch_start + self._outChannelsList[i]
                    pOutLocal[i][frame] = frame_in[:, ch_start:ch_end].flatten()
                    ch_start = ch_end
            else:
                # Deinterleaved input: reshape to (channels, samples)
                frame_in = pInLocal[frame].reshape((C_total, S))
                ch_start = 0
                for i in range(N):
                    ch_end = ch_start + self._outChannelsList[i]
                    pOutLocal[i][frame] = frame_in[ch_start:ch_end, :].flatten()
                    ch_start = ch_end

        out_lists = [pOutLocal[i].flatten().tolist() for i in range(N)]
        return out_lists
