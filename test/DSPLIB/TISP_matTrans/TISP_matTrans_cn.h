// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>

#include "TISP_matTrans.hpp"

/* Natural C matrix transpose (float and double).
 * Strides are in elements. */
void DSPLIB_matTrans_cn(void    *pIn,
                        void    *pOut,
                        uint32_t dataType,
                        size_t   widthIn,
                        size_t   heightIn,
                        size_t   strideIn,
                        size_t   strideOut);
