// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_multiplexer.hpp"

void AUDIOLIB_multiplexer_cn(void    *pIn,
                             void    *pOut,
                             size_t   widthIn,
                             size_t   heightIn,
                             size_t   inputs,
                             size_t  *prevIdx,
                             size_t  *currentIdx,
                             size_t   strideInElementsY,
                             size_t   strideInElementsZ,
                             size_t   strideOutElements,
                             uint32_t data_type);
