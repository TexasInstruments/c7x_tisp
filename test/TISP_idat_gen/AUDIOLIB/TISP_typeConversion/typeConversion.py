# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class TypeConversion:
    """Reference model of the typeConversion kernel (float <-> fixed point)."""

    def __init__(self, *, rows, cols, in_dtype, out_dtype, testQ31=0, **_):
        """Dtypes are numpy names: float32, float64, int16, int32."""
        self.rows = int(rows)
        self.cols = int(cols)
        self.in_dtype = np.dtype(in_dtype).name
        self.out_dtype = np.dtype(out_dtype).name
        self.test_q31 = int(testQ31)
        self.init()

    @staticmethod
    def _float_to_int(in0, mul_factor, min_val, max_val):
        """Convert float/double to fixed point: scale, round-half-even-ish, saturate."""
        out = np.zeros(in0.shape, dtype=np.int32)

        for i in range(in0.shape[0]):
            for j in range(in0.shape[1]):
                val = in0[i][j]
                mulVal = val * mul_factor
                intPart = int(mulVal)
                fracPart = mulVal - intPart

                if (intPart % 2 == 0) and (abs(fracPart) == 0.5):
                    aInt = intPart
                elif val < 0:
                    aInt = int(mulVal - 0.5)
                else:
                    aInt = int(mulVal + 0.5)

                # Saturate to the target integer range
                if aInt > max_val:
                    aInt = max_val
                if aInt < min_val:
                    aInt = min_val

                out[i][j] = aInt

        return out

    def _float_params(self):
        """(np_in, mul, lo, hi) for float/double -> int16_t/int32_t.

        Anything not listed falls back to the double -> Q23 int32 conversion.
        """
        in_dtype, out_dtype, test_q31 = self.in_dtype, self.out_dtype, self.test_q31
        if in_dtype == "float32" and out_dtype == "int16":
            return np.float32, 32768.0, -32768, 32767
        if in_dtype == "float64" and out_dtype == "int16":
            return np.double, 32768.0, -32768, 32767
        if in_dtype == "float32" and out_dtype == "int32" and test_q31 == 0:
            return np.float32, 8388608.0, -8388608, 8388607
        if in_dtype == "float32" and out_dtype == "int32" and test_q31 == 1:
            return np.float32, 2147483648, -2147483648, 2147483647
        if in_dtype == "float64" and out_dtype == "int32" and test_q31 == 1:
            return np.double, 2147483648, -2147483648, 2147483647
        return np.double, 8388608.0, -8388608, 8388607

    def init(self):
        """Resolve the conversion mode and scale/limits once (called by the constructor)."""
        self.fixed_to_float = self.in_dtype in ("int16", "int32")
        if self.fixed_to_float:
            if self.in_dtype == "int16":
                self.div = 32768.0
            else:
                self.div = 8388608.0 if self.test_q31 == 0 else 2147483648.0
            self.np_out = np.float32 if self.out_dtype == "float32" else np.float64
        else:
            _, self.mul, self.lo, self.hi = self._float_params()

    def exec(self, in0):
        """Convert one frame (rows x cols) as the kernel does."""
        if self.fixed_to_float:
            return in0.astype(self.np_out) / self.div
        return self._float_to_int(in0, self.mul, self.lo, self.hi)

    def generate_inputs(self, num_blocks=1):
        """SDK test signal (for gen_data.py only).

        Returns an input buffer of shape (rows * num_blocks, cols).
        """
        shape = (self.rows * num_blocks, self.cols)

        if self.in_dtype == "int16":
            return np.random.uniform(low=-32768, high=32767, size=shape).astype("int16")
        if self.in_dtype == "int32":
            if self.test_q31 == 0:
                lo, hi = -8388608, 8388607
            else:
                lo, hi = -2147483648, 2147483647
            return np.random.uniform(low=lo, high=hi, size=shape).astype("int32")
        np_in = self._float_params()[0]
        return np.random.uniform(low=-1.0, high=1.1, size=shape).astype(np_in)

