// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_mute.hpp"

void AUDIOLIB_mute_cn_set(uint32_t isMute,
                          uint32_t fadeType,
                          float    fadeTime,
                          int32_t  samplingRate,
                          size_t   isInterleaved);

void AUDIOLIB_mute_cn(void    *pIn,
                      void    *pOut,
                      size_t   widthIn,
                      size_t   heightIn,
                      size_t   strideInElements,
                      size_t   strideOutElements,
                      size_t   isInterleaved,
                      int32_t  bypassSmoothing,
                      uint32_t fadeType);
