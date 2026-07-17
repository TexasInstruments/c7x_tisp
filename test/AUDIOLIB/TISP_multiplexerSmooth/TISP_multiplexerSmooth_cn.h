// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_multiplexerSmooth.hpp"

void AUDIOLIB_multiplexerSmooth_cn(void    *pIn0,
                                   void    *pIn1,
                                   void    *pIn2,
                                   void    *pIn3,
                                   void    *pOut,
                                   size_t   width,
                                   size_t   height,
                                   size_t   inputs,
                                   size_t   strideInElements,
                                   size_t   strideOutElements,
                                   size_t  *prevIdx,
                                   size_t  *currentIdx,
                                   size_t   maxSmoothedSamples,
                                   size_t   isInterleave,
                                   uint32_t data_type);
