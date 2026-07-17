// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_subBlockStatistics.hpp"

void AUDIOLIB_subBlockStatistics_cn(void    *pIn,
                                    void    *pOut,
                                    uint32_t data_type,
                                    size_t   widthIn,
                                    size_t   heightIn,
                                    size_t   subBlockSize,
                                    size_t   strideInElements,
                                    size_t   strideOutElements,
                                    size_t   flagPerChanStats,
                                    size_t   statisticsType);
