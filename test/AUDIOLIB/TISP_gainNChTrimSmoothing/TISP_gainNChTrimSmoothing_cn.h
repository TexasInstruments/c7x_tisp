// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_gainNChTrimSmoothing.hpp"

void AUDIOLIB_gainNChTrimSmoothing_cn_set(float   *pInitialGain,
                                          float   *pTrimGain,
                                          float    masterGain,
                                          float    fadeTime,
                                          uint32_t fadeType,
                                          float    samplingRate,
                                          size_t   numChannels);
void AUDIOLIB_gainNChTrimSmoothing_cn(void    *pIn,
                                      void    *pOut,
                                      uint32_t data_type,
                                      size_t   widthIn,
                                      size_t   heightIn,
                                      size_t   strideIn,
                                      size_t   strideOut,
                                      size_t   isInterleave);
