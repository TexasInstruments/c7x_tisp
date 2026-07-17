import numpy as np


def tableInterpolateProcess(src, table, minVal, maxVal, tableSamples):
    L = len(table)
    dst = np.empty_like(src)

    # Calculate divisor like in C code
    divisor = (tableSamples - 1) / (maxVal - minVal + 1e-9)

    for i, x in enumerate(src):
        x = (x - minVal) * divisor
        if x <= 0:
            dst[i] = table[0]
        elif x >= (tableSamples - 1):
            dst[i] = table[L - 1]
        else:
            index = np.floor(x)
            fract = x - index
            index = int(index)
            dst[i] = (table[index + 1] * fract) + (table[index] * (1 - fract))

    return dst
