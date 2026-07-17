// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_MULTIPLEXERSMOOTH_IXX_IXX_OXX_IDAT_H

#define TISP_MULTIPLEXERSMOOTH_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t testPattern; /* 0: constant, 1: sequential, 2: random, 3: static
                           array, 4: file, etc */
   void    *staticIn0;
   void    *staticIn1;
   void    *staticIn2;
   void    *staticOut;
   uint32_t dataType;
   uint32_t inputs;
   uint32_t inSamples;
   uint32_t inChannels;
   uint32_t currentIdx;
   int32_t  samplingRate;
   float    currentFadeTime;
   uint8_t  isInterleave;
   uint32_t strideIn;
   uint32_t strideOut;
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t numBlocks;
   uint32_t testID;

} TISP_multiplexerSmooth_testParams_t;

void TISP_multiplexerSmooth_getTestParams(TISP_multiplexerSmooth_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_MULTIPLEXERSMOOTH_IXX_IXX_OXX_IDAT_H */
