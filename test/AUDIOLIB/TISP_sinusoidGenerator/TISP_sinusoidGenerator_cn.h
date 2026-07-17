// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_sinusoidGenerator.hpp"

void AUDIOLIB_sinusoidGenerator_cn_set(float   startPhase,
                                       float   startFrequency,
                                       float   targetFrequency,
                                       int32_t samplingRate,
                                       float   smoothingTime);

void AUDIOLIB_sinusoidGenerator_cn(void *pOut, uint32_t data_type, size_t widthIn);
