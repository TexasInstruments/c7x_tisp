# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


# SDK C type names (as used in the test-case CSV files) -> numpy dtype names
_C_TO_NP_DTYPE = {"float": "float32", "double": "float64"}


class DelayNChannel:
    """Reference model of the delayNChannel kernel (per-channel sample delay).

    One exec() call processes one frame, in the kernel's layout: [num_samples, num_channels]
    when is_interleaved, else [num_channels, num_samples]. The delay line is kept in the object.
    """

    def __init__(self, *, num_channels, num_samples, in_dtype, is_interleaved, mode, pDelaySize, **_):
        if mode not in (0, 1):
            raise ValueError(f"Unsupported mode: {mode}")
        self.mode = int(mode)
        self.dType = np.dtype(in_dtype)
        self.interleave = int(bool(is_interleaved))
        self.numChannels = int(num_channels)
        self.numSamples = int(num_samples)
        self.delaySize = np.asarray(pDelaySize).astype(int).reshape(-1)
        self.init()

    @staticmethod
    def generate_inputs(dType, interleave, numChannels, numSamples, maxDelay, numBlocks):
        """SDK test signal: random input for numBlocks frames and per-channel delay sizes.

        Only gen_data.py calls this, before building the model (the delay sizes are a
        constructor argument). Draws from np.random in a fixed order.

        Args:
            dType: SDK C type name ("float" / "double") or numpy name.
            interleave: 1 for interleaved layout.

        Returns:
            (inputBuff, delaySize): inputBuff is in the kernel's layout,
            [numBlocks * numSamples, numChannels] when interleaved, else
            [numChannels, numBlocks * numSamples]; delaySize is uint32 [numChannels].
        """
        npType = _C_TO_NP_DTYPE.get(dType, dType)
        if npType not in ("float32", "float64"):
            raise ValueError(f"Unsupported data type: {dType}")
        # range of the 32-bit signed integer type, as in the SDK test
        minVal = -(2 ** 31)
        maxVal = 2 ** 31

        minDelaySize = 1
        maxDelaySize = maxDelay

        if interleave == 1:
            inputBuff = np.random.uniform(
                low=minVal, high=maxVal, size=(numBlocks * numSamples, numChannels)
            ).astype(npType)
        else:
            inputBuff = np.random.randint(
                low=minVal, high=maxVal, size=(numChannels, numBlocks * numSamples)
            ).astype(npType)

        delaySize = np.random.randint(
            low=minDelaySize, high=maxDelaySize, size=(numChannels)
        ).astype("uint32")
        if maxDelay not in delaySize:
            delaySize[numChannels - 1] = maxDelay
        np.random.shuffle(delaySize)

        return inputBuff, delaySize

    def init(self):
        """Create the persistent state (delay buffer and indices) for the kernel."""
        numChannels = self.numChannels
        numSamples = self.numSamples

        # The maximum delay comes from the per-channel delay sizes (pDelaySize)
        maxDelay = int(np.max(self.delaySize))

        if self.mode == 0:
            delayBuffSize = numSamples + maxDelay
        else:
            if (numSamples + maxDelay) < 512:
                delayBuffSize = 512
            else:
                delayBuffSize = 2 ** int(np.ceil(np.log2(numSamples + maxDelay)))

        self.delayBuffSize = delayBuffSize
        self.delayBuff = np.zeros((numChannels, delayBuffSize), dtype=self.dType)
        self.readIdx = [0] * numChannels
        self.writeIdx = [int(x) for x in self.delaySize]

    def reset(self):
        """Return to the initial (zero) state."""
        self.init()

    def exec(self, in_frame):
        """Process one frame; updates the delay line in the object.

        Args:
            in_frame: [num_samples, num_channels] if interleaved, else [num_channels, num_samples].

        Returns:
            out frame in the same layout as in_frame.
        """
        if self.interleave:
            return self._exec_planar(np.asarray(in_frame).T).T
        return self._exec_planar(np.asarray(in_frame))

    def _exec_planar(self, in_frame):
        """Process one planar frame (numChannels, numSamples)."""
        mode = self.mode
        numChannels = self.numChannels
        numSamples = self.numSamples
        delaySize = self.delaySize
        delayBuff = self.delayBuff
        delayBuffSize = self.delayBuffSize
        readIdx = self.readIdx
        writeIdx = self.writeIdx

        out_frame = np.zeros((numChannels, numSamples), dtype=in_frame.dtype)

        if mode == 0:
            for ch in range(numChannels):
                if delaySize[ch] < numSamples:
                    for i in range(delaySize[ch]):
                        out_frame[ch, i] = delayBuff[ch, i]
                    for i in range(numSamples - delaySize[ch]):
                        out_frame[ch, delaySize[ch] + i] = in_frame[ch, i]
                    if delaySize[ch] > 0:
                        for i in range(delaySize[ch]):
                            delayBuff[ch, i] = in_frame[
                                ch, (numSamples - delaySize[ch]) + i
                            ]
                else:
                    for i in range(numSamples):
                        out_frame[ch, i] = delayBuff[ch, i]
                    for i in range(delaySize[ch] - numSamples):
                        delayBuff[ch, i] = delayBuff[ch, numSamples + i]
                    for i in range(numSamples):
                        delayBuff[ch, (delaySize[ch] - numSamples) + i] = in_frame[
                            ch, i
                        ]
        elif mode == 1:
            for ch in range(numChannels):
                # Write Delay Buffer
                writeSampleCnt1 = min((delayBuffSize - writeIdx[ch]), numSamples)
                writeSampleCnt2 = numSamples - writeSampleCnt1
                for i in range(writeSampleCnt1):
                    delayBuff[ch, writeIdx[ch] + i] = in_frame[ch, i]
                if (writeIdx[ch] + numSamples) >= delayBuffSize:
                    writeIdx[ch] = 0
                else:
                    writeIdx[ch] = writeIdx[ch] + numSamples
                if writeSampleCnt2 > 0:
                    for i in range(writeSampleCnt2):
                        delayBuff[ch, i] = in_frame[
                            ch, (writeSampleCnt1 - writeIdx[ch]) + i
                        ]
                    writeIdx[ch] = writeSampleCnt2

                # Read Delay Buffer
                readSampleCnt1 = min((delayBuffSize - readIdx[ch]), numSamples)
                readSampleCnt2 = numSamples - readSampleCnt1
                for i in range(readSampleCnt1):
                    out_frame[ch, i] = delayBuff[ch, readIdx[ch] + i]
                if (readIdx[ch] + numSamples) >= delayBuffSize:
                    readIdx[ch] = 0
                else:
                    readIdx[ch] = readIdx[ch] + numSamples

                if readSampleCnt2 > 0:
                    for i in range(readSampleCnt2):
                        out_frame[ch, (readSampleCnt1 - readIdx[ch]) + i] = delayBuff[
                            ch, i
                        ]
                    readIdx[ch] = readSampleCnt2

        return out_frame
