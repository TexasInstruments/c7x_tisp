// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_typeConversion.hpp"

void AUDIOLIB_typeConversion_cn(void    *pIn,
                                void    *pOut,
                                uint32_t indata_type,
                                uint32_t outdata_type,
                                size_t   widthIn,
                                size_t   heightIn,
                                size_t   strideInElements,
                                size_t   strideOutElements,
                                size_t   testQ31);
