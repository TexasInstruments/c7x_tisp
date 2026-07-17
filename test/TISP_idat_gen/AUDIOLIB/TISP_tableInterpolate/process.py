import numpy as np


def tableInterpolateProcess(src, table, minVal, maxVal, tableSamples):

    # Force everything to float32 exactly like C
    src = src.astype(np.float32)
    table = table.astype(np.float32)
    minVal = np.float32(minVal)
    maxVal = np.float32(maxVal)
    tableSamples = int(tableSamples)

    # Same divisor as C (float32)
    divisor = np.float32((tableSamples - 1) / (maxVal - minVal + 1e-9))

    dst = np.empty_like(src, dtype=np.float32)
    L = len(table)

    for i in range(len(src)):

        x = np.float32(src[i] - minVal)
        x = np.float32(x * divisor)

        if x <= 0:
            dst[i] = table[0]

        elif x >= (tableSamples - 1):
            dst[i] = table[L - 1]

        else:
            # floorf(x) exactly
            index = int(np.floor(x).astype(np.float32))

            # fract = x - index (float32)
            fract = np.float32(x - np.float32(index))

            # interpolation (float32)
            dst[i] = np.float32(
                table[index] * np.float32(1.0 - fract) + table[index + 1] * fract
            )

    return dst
