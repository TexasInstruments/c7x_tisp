// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_router.hpp"

void AUDIOLIB_router_cn_set(void     *pC,
                            size_t    heightIn,
                            size_t    cheightIn,
                            size_t    strideIn,
                            size_t    isInterleave,
                            int32_t  *muteArray,
                            uint32_t *outputOffsetArray);

void AUDIOLIB_router_cn(void     *pIn,
                        void     *pOut,
                        void     *pScratch,
                        uint32_t  data_type,
                        size_t    widthIn,
                        size_t    heightIn,
                        size_t    strideIn,
                        size_t    strideOut,
                        size_t    cheightIn,
                        int32_t  *muteArray,
                        uint32_t *outputOffsetArray,
                        size_t    isInterleave);
