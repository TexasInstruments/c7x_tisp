// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_GAINNCHTRIMSMOOTHING_IXX_IXX_OXX_IDAT_H

#define TISP_GAINNCHTRIMSMOOTHING_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern; /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc.*/
   void    *staticIn0;
   void    *staticIn1;
   void    *staticOut;
   uint32_t widthIn;
   uint32_t heightIn;
   uint32_t isInterleave;
   uint32_t bypassMode;
   float    samplingRate;
   float    fadeTime;
   uint32_t fadeType;
   float    masterGain;
   int32_t  strideInElements;
   int32_t  strideOutElements;
   uint32_t dataType;
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t numBlocks;
   uint32_t testID;

} TISP_gainNChTrimSmoothing_testParams_t;

void TISP_gainNChTrimSmoothing_getTestParams(TISP_gainNChTrimSmoothing_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_GAINNCHTRIMSMOOTHING_IXX_IXX_OXX_IDAT_H */
