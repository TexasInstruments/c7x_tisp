// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_agc.hpp"

void AUDIOLIB_agc_cn_set(int32_t samplingRate,
                         float   currentGain,
                         float   ratio,
                         float   smoothingTime,
                         float   recoveryRate,
                         size_t  numSamples,
                         size_t  numChannels);
void AUDIOLIB_agc_cn(void    *pIn,
                     void    *pOut,
                     uint32_t data_type,
                     size_t   widthIn,
                     size_t   heightIn,
                     size_t   strideIn,
                     size_t   strideOut,
                     float    targetLevel,
                     float    maxAttenuation,
                     float    maxGain,
                     float    activationThreshold,
                     uint32_t enableRecovery);
