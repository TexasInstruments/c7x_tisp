// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_balanceFader.hpp"

void AUDIOLIB_balanceFader_cn_set(void    *pG,
                                  float    balance,
                                  float    fade,
                                  float    sideGainFactor,
                                  size_t   heightIn,
                                  int32_t *channelConfig,
                                  int32_t  lfeBalanceMode,
                                  float    lfeBalanceGainFactor,
                                  int32_t  lfeFaderMode,
                                  float    lfeFaderGainFactor);

void AUDIOLIB_balanceFader_cn(void    *pIn,
                              void    *pGain,
                              void    *pOut,
                              uint32_t data_type,
                              size_t   widthIn,
                              size_t   heightIn,
                              size_t   strideIn,
                              size_t   strideOut,
                              size_t   isInterleave);
