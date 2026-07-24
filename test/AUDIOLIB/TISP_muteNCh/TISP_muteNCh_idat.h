// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_MUTENCH_IXX_IXX_OXX_IDAT_H

#define TISP_MUTENCH_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t   testPattern; /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc.*/
   void     *staticIn0;
   void     *staticOut;
   uint32_t  dataType;
   uint32_t  widthIn;
   uint32_t  heightIn;
   uint32_t *isMute;
   int32_t   samplingRate;
   float     fadeTime;
   uint8_t   isInterleaved;
   uint32_t  fadeType;   // 0=LINEAR, 1=SMOOTH, 2=HARD
   uint32_t  bypassMode; // 0=apply mute, 1=copy input to output
   uint32_t  strideIn;
   uint32_t  strideOut;
   uint32_t  outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t  numReps;
   int32_t   numBlocks; // number of blocks to process
   uint32_t  testID;
} TISP_muteNCh_testParams_t;

void TISP_muteNCh_getTestParams(TISP_muteNCh_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_MUTENCH_IXX_IXX_OXX_IDAT_H */
