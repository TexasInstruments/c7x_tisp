import numpy as np


def tableLookupProcess(src, table, minVal, maxVal, tableSamples):
    if np.issubdtype(src.dtype, np.integer):
        # int32_t source: each sample is used directly as the table index
        src32 = src.astype(np.int32)

        dst = np.empty(src32.shape, dtype=table.dtype)

        for i, x in enumerate(src32):
            if x <= 0:
                dst[i] = table[0]
            elif x >= (tableSamples - 1):
                dst[i] = table[tableSamples - 1]
            else:
                dst[i] = table[x]

    elif np.issubdtype(src.dtype, np.floating):
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
