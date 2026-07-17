import numpy as np


def tableLookupProcess(src, table, minVal, maxVal, tableSamples):
    src32 = src.astype(np.float32)
    table32 = table.astype(np.float32)
    divisor = np.float32((tableSamples - 1) / (maxVal - minVal + 1e-9))

    dst = np.empty_like(src32)

    for i, x in enumerate(src32):
        sampleSrc = np.float32(x - np.float32(minVal))
        sampleSrc = np.float32(sampleSrc * divisor)
        sampleSrc = np.float32(sampleSrc + np.float32(0.5))

        if sampleSrc <= 0:
            dst[i] = table32[0]
        elif sampleSrc >= (tableSamples - 1):
            dst[i] = table32[tableSamples - 1]
        else:
            idx = int(np.floor(sampleSrc))  # floor(float32)
            dst[i] = table32[idx]

    return dst
