// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_nlms.hpp"

void AUDIOLIB_nlms_cn(void    *pIn,
                      void    *pInDesired,
                      void    *pStateBuffer,
                      void    *pScratchBuffer,
                      void    *pCoefficients,
                      void    *pOut,
                      size_t   width,
                      size_t   height,
                      size_t   totalSamples,
                      size_t   filterLength,
                      float    stepSize,
                      size_t   circBuffMask,
                      float    regularization,
                      size_t   strideInElements,
                      size_t   strideOutElements,
                      size_t   strideStateElements,
                      uint32_t data_type,
                      uint32_t isInterleave);
