// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_WHITENOISE_IXX_IXX_OXX_IDAT_H

#define TISP_WHITENOISE_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern; /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc.*/
   void    *staticOut;
   uint32_t numSamples;
   uint32_t seed;
   float    range;
   uint32_t dataType;
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t testID;
   uint32_t numBlocks;

} TISP_whiteNoise_testParams_t;

void TISP_whiteNoise_getTestParams(TISP_whiteNoise_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_WHITENOISE_IXX_IXX_OXX_IDAT_H */
