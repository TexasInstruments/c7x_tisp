// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_softClip.hpp"

void AUDIOLIB_softClip_cn(void  *pIn,
                          void  *pOut,
                          size_t widthIn,
                          size_t heightIn,
                          size_t strideInElements,
                          size_t strideOutElements,
                          float  threshold,
                          float  endKnee);
