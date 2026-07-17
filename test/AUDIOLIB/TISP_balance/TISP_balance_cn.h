// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_balance.hpp"

void AUDIOLIB_balance_cn(void    *pInL,
                         void    *pInR,
                         void    *pOutL,
                         void    *pOutR,
                         uint32_t data_type,
                         size_t   widthIn,
                         size_t   heightIn,
                         size_t   strideInElements,
                         size_t   strideOutElements,
                         float    balance,
                         int32_t  samplingrate,
                         float    smoothingTime);
