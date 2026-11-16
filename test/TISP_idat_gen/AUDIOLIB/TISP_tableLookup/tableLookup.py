# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class TableLookup:
    """Reference model of the tableLookup kernel (nearest-entry table lookup with clamping)."""

    def __init__(self, *, table_samples, min_val, max_val, **_):
        self.table_samples = int(table_samples)
        self.min_val = float(min_val)
        self.max_val = float(max_val)
        self.init()

    def init(self):
        """Precompute the float index scaling divisor (float32, as in C)."""
        self.divisor = np.float32(
            (self.table_samples - 1) / (self.max_val - self.min_val + 1e-9)
        )

    def generate_inputs(
        self, src_samples, num_blocks=1, src_dtype="float", table_dtype="float"
    ):
        """Generate random source and table data (test data only); returns (src, table)."""
        minVal = self.min_val
        maxVal = self.max_val
        tableSamples = self.table_samples
        if src_dtype == "float":
            src = np.random.uniform(
                low=minVal - 10, high=maxVal, size=src_samples * num_blocks
            )
            table = np.random.uniform(low=minVal - 10, high=maxVal, size=tableSamples)
        else:
            # indices from -10 to tableSamples + 10, so both clamps and in-range entries are exercised
            src = np.random.randint(
                low=-10,
                high=tableSamples + 11,
                size=src_samples * num_blocks,
                dtype=np.int32,
            )
            if table_dtype == "float":
                table = np.random.uniform(low=-1.0, high=1.0, size=tableSamples).astype(
                    np.float32
                )
            else:
                table = np.random.randint(
                    low=-1000, high=1000, size=tableSamples, dtype=np.int32
                )
        return src, table

    def exec(self, src, table):
        """Look up each src sample in table; returns dst."""
        tableSamples = self.table_samples
        minVal = self.min_val
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
            divisor = np.float32(
                (tableSamples - 1) / (self.max_val - minVal + 1e-9)
            )

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
