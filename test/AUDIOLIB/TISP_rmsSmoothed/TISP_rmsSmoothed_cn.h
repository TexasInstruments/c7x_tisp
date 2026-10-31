// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_rmsSmoothed.hpp"

void AUDIOLIB_rmsSmoothed_cn_set(int32_t samplingRate, float smoothingTime, size_t widthIn);
void AUDIOLIB_rmsSmoothed_cn(void    *pIn,
                             void    *pOut,
                             uint32_t data_type,
                             size_t   widthIn,
                             size_t   heightIn,
                             size_t   strideIn);
