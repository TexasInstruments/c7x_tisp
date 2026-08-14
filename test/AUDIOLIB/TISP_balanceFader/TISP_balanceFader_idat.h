// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_BALANCEFADER_IXX_IXX_OXX_IDAT_H

#define TISP_BALANCEFADER_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern; /* 0: constant, 1: sequential, 2: random, 3: static array, 4: file, etc */
   void    *staticIn;
   void    *staticOut;
   uint32_t dataType;
   uint32_t widthIn;
   uint32_t heightIn;
   float    balance;
   float    fade;
   float    sideGainFactor;
   float    lfeBalanceGainFactor;
   float    lfeFaderGainFactor;
   int32_t  lfeBalanceMode;
   int32_t  lfeFaderMode;
   int32_t *channelConfig;
   uint8_t  isInterleave;
   uint32_t bypassMode;
   uint32_t strideIn;
   uint32_t strideOut;
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t numBlocks;
   uint32_t testID;

} TISP_balanceFader_testParams_t;

void TISP_balanceFader_getTestParams(TISP_balanceFader_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_BALANCEFADER_IXX_IXX_OXX_IDAT_H */
