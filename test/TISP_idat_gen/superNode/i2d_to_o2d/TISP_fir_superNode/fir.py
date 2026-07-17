import numpy as np


class FIR:
    def __init__(self):
        """
        Initializes the FIR class.

        Args:
            None

        Returns:
            None
        """
        pass

    def execute(
        self,
        inputData,
        stateData,
        filterData,
        batchSize,
        dataSize,
        filterSize,
        numBlocks,
        enableNChCoefs,
    ):
        """
        Perform the FIR operation on the input data.

        Args:
            inputData (ndarray): The input data to be processed.
            stateData (ndarray): The state data.
            filterData (ndarray): The filter data.
            batchSize (int): The number of batches.
            dataSize (int): The size of the data.
            filterSize (int): The size of the filter.
            numBlocks (int): The number of blocks.
            enableNChCoefs (int): Flag to enable per channel filter coefficients.

        Returns:
            ndarray: The output data after performing the FIR operation.
        """
        outputData = []

        # copy to state
        stateData[:, filterSize - 1 :] = inputData

        # perform FIR operation
        if enableNChCoefs == 1:
            for ch in range(batchSize):
                outputData.append(
                    np.convolve(
                        stateData[ch, 0 : (dataSize * numBlocks + filterSize - 1)],
                        filterData[ch, 0:filterSize],
                        mode="valid",
                    )
                )

        else:
            for ch in range(batchSize):
                outputData.append(
                    np.convolve(
                        stateData[ch, 0 : (dataSize * numBlocks + filterSize - 1)],
                        filterData[0, 0:filterSize],
                        mode="valid",
                    )
                )

        return np.array(outputData)
