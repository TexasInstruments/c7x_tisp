// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_delayNChannel.hpp"

void AUDIOLIB_delayNChannel_cn(void     *pIn,
                               void     *pDelay,
                               void     *pScratch,
                               void     *pOut,
                               uint32_t  data_type,
                               size_t    widthIn,
                               size_t    heightIn,
                               size_t    interleave,
                               size_t    mode,
                               uint32_t *delaySize,
                               size_t    delayBuffSize,
                               size_t    strideInElements,
                               size_t    strideOutElements,
                               size_t    strideDelayElements);
