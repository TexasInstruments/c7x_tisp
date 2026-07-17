// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_ATAN2_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_ATAN2_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern; /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc.*/
   void    *staticInLeft;
   void    *staticOutLeft;
   void    *staticInRight;
   void    *staticOutRight;
   uint32_t dataType;
   uint32_t widthIn;
   uint32_t heightIn;
   float    balance;
   int32_t  samplingRate;
   float    smoothingTime;
   int32_t  strideInElements;
   int32_t  strideOutElements;
   uint32_t blockWidth;
   uint32_t blockHeight;
   uint32_t testID;
   uint32_t numBlocks;
} TISP_balance_superNode_testParams_t;

void TISP_balance_superNode_getTestParams(TISP_balance_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_ATAN2_SUPERNODE_IXX_IXX_OXX_IDAT_H */
