// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_limiter.hpp"

template <typename dataType>
void TISP_limiter_exec_cn(void  *pIn,
                          void  *pGain,
                          void  *pScratch,
                          void  *pOut,
                          void  *pThreshold_dB,
                          void  *pKneeWidth_dB,
                          void  *pAttackCoeff,
                          void  *pReleaseCoeff,
                          size_t numChannels,
                          size_t blockSize,
                          size_t pitchX,
                          size_t pitchS,
                          size_t pitchY,
                          bool   isInterleaved);
