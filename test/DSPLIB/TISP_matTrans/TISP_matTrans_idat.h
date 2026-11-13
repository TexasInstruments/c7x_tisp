// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_MATTRANS_IXX_OXX_IDAT_H

#define TISP_MATTRANS_IXX_OXX_IDAT_H

#include <dsplib.h>

#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern; /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc.*/
   void    *staticIn;
   void    *staticOut;
   uint32_t dataType;
   uint32_t widthIn;
   uint32_t heightIn;
   int32_t  strideIn; // bytes
   uint32_t widthOut;
   uint32_t heightOut;
   int32_t  strideOut;          // bytes
   uint32_t numBlocks;          // independent back-to-back blocks per case
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t testID;
} TISP_matTrans_testParams_t;

void TISP_matTrans_getTestParams(TISP_matTrans_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_MATTRANS_IXX_OXX_IDAT_H */
