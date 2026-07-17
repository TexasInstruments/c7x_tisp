// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_BALANCEFADER_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_BALANCEFADER_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern;
   void    *staticIn0;
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
   int32_t  strideIn;
   int32_t  strideOut;
   uint32_t blockWidth;
   uint32_t blockHeight;
   uint32_t testID;
} TISP_balanceFader_superNode_testParams_t;

void TISP_balanceFader_superNode_getTestParams(TISP_balanceFader_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_BALANCEFADER_SUPERNODE_IXX_IXX_OXX_IDAT_H */
