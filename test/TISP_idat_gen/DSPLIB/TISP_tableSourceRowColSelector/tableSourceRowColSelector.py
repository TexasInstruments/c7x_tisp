# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class TableSourceRowColSelector:
    """Reference model of the tableSourceRowColSelector kernel (one table row or column per index)."""

    def __init__(
        self,
        num_indices: int,
        num_rows: int,
        num_cols: int,
        is_col_select: int,
        is_output_interleave: int,
        num_blocks: int,
        dtype: str,
    ):
        if dtype[0] == "f":
            self.np_dtype = "float32"
        elif dtype[0] == "d":
            self.np_dtype = "float64"
        else:
            self.np_dtype = dtype[:-2]

        self.num_indices = num_indices
        self.num_rows = num_rows
        self.num_cols = num_cols
        self.is_col_select = is_col_select
        self.is_output_interleave = is_output_interleave
        self.num_blocks = num_blocks
        self.dtype = dtype
        # Number of selectable table vectors: rows for row select, columns for column select
        self.num_sel = num_cols if is_col_select == 1 else num_rows

    def generate_inputs(self):
        """Generate the index buffer and the table.

        Returns:
            (in0, in1): in0 holds one row of numIndices int32 indices per block, shape
            (numBlocks, numIndices); the indices span below 0 and past the last selectable
            row or column to exercise clamping. in1 is the table, shape (numRows, numCols).
        """
        in0 = np.random.randint(
            low=-3, high=self.num_sel + 3, size=(self.num_blocks, self.num_indices)
        ).astype("int32")

        if self.dtype[0] == "f" or self.dtype[0] == "d":
            [minVal, maxVal] = [-10, 10]
            in1 = np.random.uniform(
                low=minVal, high=maxVal, size=(self.num_rows, self.num_cols)
            ).astype(self.np_dtype)
        else:
            # [-1000, 1000] clipped to the type
            [minVal, maxVal] = [
                max(np.iinfo(self.np_dtype).min, -1000),
                min(np.iinfo(self.np_dtype).max, 1000),
            ]
            in1 = np.random.randint(
                low=minVal, high=maxVal + 1, size=(self.num_rows, self.num_cols)
            ).astype(self.np_dtype)

        return in0, in1

    def execute(self, in0: np.ndarray, in1: np.ndarray) -> np.ndarray:
        """Select one table row or column per index, the index clamped to the table.

        Returns:
            The outputs of all blocks stacked by rows:
            non-interleaved -> (numBlocks * numIndices, selLen), row i is the vector of index i
            interleaved     -> (numBlocks * selLen, numIndices), column i is the vector of index i
        """
        outs = []
        for block in in0:
            sel = np.clip(block, 0, self.num_sel - 1)
            vecs = in1[:, sel].T if self.is_col_select == 1 else in1[sel, :]
            outs.append(vecs.T if self.is_output_interleave == 1 else vecs)
        return np.concatenate(outs, axis=0)
