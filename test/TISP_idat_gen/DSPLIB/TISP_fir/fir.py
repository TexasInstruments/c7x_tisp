# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np

# dTypeIn -> numpy dtype used for the generated input and filter
_NP_DTYPES = {
    "int8_t": "int8",
    "int16_t": "int16",
    "int32_t": "int32",
    "int64_t": "int64",
    "uint8_t": "uint8",
    "uint16_t": "uint16",
    "uint32_t": "uint32",
    "uint64_t": "uint64",
    "float": "float",
    "double": "double",
}


class FIR:
    """Reference model of the FIR kernel (stateful, processed frame by frame).

    One exec() call filters one frame [num_channels, num_samples]; the delay line
    (the last filterSize-1 input samples per channel) is kept in the object.
    """

    def __init__(self, *, num_channels, num_samples, in_dtype, filterSize, pCoeffs, bypassMode=0, **_):
        self.npDtype = np.dtype(in_dtype)
        self.numChannels = int(num_channels)
        self.numSamples = int(num_samples)
        self.filterSize = int(filterSize)
        self.bypassMode = int(bypassMode)

        # pCoeffs is the kernel's buffer: one set, or num_channels sets, of filterSize taps
        # (in the order the kernel receives them, i.e. already reversed along the tap axis)
        self.filterData = np.asarray(pCoeffs, dtype=self.npDtype).reshape(-1, self.filterSize)
        self.enableNChCoefs = int(self.filterData.shape[0] == self.numChannels and self.numChannels > 1)

        self.init()

    def init(self):
        """Zero-initialised filter history: the last filterSize-1 input samples per channel."""
        self.state = np.zeros((self.numChannels, self.filterSize - 1)).astype(self.npDtype)

    def reset(self):
        """Return to the initial (zero) state."""
        self.init()

    @staticmethod
    def generate_inputs(dTypeIn, dataSize, filterSize, batchSize, enableNChCoefs, numBlocks):
        """SDK test signal: FIR input for all frames and the filter coefficients.

        Only gen_data.py calls this, before building the model (the taps are a
        constructor argument). Draws from np.random in a fixed order.

        Returns:
            (inputData, filterData): inputData has shape (batchSize, dataSize * numBlocks).
            filterData has shape (1 or batchSize, filterSize), reversed along the tap axis
            as the kernel (and the header writer) expect; it is what the model takes as pCoeffs.
        """
        if dTypeIn not in _NP_DTYPES:
            raise ValueError(f"Unsupported data type: {dTypeIn}")
        npDtype = _NP_DTYPES[dTypeIn]

        if dTypeIn == "int8_t" or dTypeIn == "uint8_t":
            bits = 8
        elif dTypeIn == "int16_t" or dTypeIn == "uint16_t":
            bits = 16
        elif dTypeIn == "int32_t" or dTypeIn == "uint32_t" or dTypeIn == "float":
            bits = 32
        else:
            bits = 64

        # unsigned integer
        if dTypeIn[0] == "u":
            minVal = 0
            maxVal = (2**bits) - 1
        else:
            minVal = -(2 ** (bits - 1))
            maxVal = 2 ** (bits - 1)

        # determine filter height
        if enableNChCoefs == 1:
            filterHeight = batchSize
        else:
            filterHeight = 1

        if dTypeIn == "float" or dTypeIn == "double":
            minVal, maxVal = -1, 1

        inputData = np.random.uniform(
            low=minVal, high=maxVal, size=(batchSize, (dataSize * numBlocks))
        ).astype(npDtype)
        filterData = np.random.uniform(
            low=minVal, high=maxVal, size=(filterHeight, filterSize)
        ).astype(npDtype)

        filterData = filterData[:, ::-1]

        return inputData, filterData

    def exec(self, frameIn):
        """Process ONE frame of FIR and update the filter history.

        Args:
            frameIn: (num_channels, num_samples) input samples of this frame.

        Returns:
            frameOut of shape (num_channels, num_samples). The history is updated even in
            bypass mode, so it always tracks the input history.
        """
        batchSize, filterSize = frameIn.shape[0], self.filterSize

        stateData = np.concatenate((self.state, frameIn), axis=1)

        frameOut = []
        for ch in range(batchSize):
            taps = self.filterData[ch if self.enableNChCoefs == 1 else 0, 0:filterSize]
            frameOut.append(np.convolve(stateData[ch], taps, mode="valid"))

        # bypass copies the input to the output, leaving the filter unapplied
        if self.bypassMode:
            frameOut = [frameIn[ch] for ch in range(batchSize)]

        self.state = stateData[:, stateData.shape[1] - (filterSize - 1):]

        return np.array(frameOut)
