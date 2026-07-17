// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_realImagToComplex.hpp"

void DSPLIB_realImagToComplex_cn(void    *pIn0,
                                 void    *pIn1,
                                 void    *pOut,
                                 uint32_t data_type,
                                 size_t   widthIn,
                                 size_t   heightIn,
                                 size_t   strideIn,
                                 size_t   strideOut);
