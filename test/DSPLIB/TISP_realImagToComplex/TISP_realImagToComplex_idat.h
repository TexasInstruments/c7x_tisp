// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_REALIMAGTOCOMPLEX_IXX_IXX_OXX_IDAT_H

#define TISP_REALIMAGTOCOMPLEX_IXX_IXX_OXX_IDAT_H

#include <dsplib.h>

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
   int32_t  strideIn;
   int32_t  strideOut;
   uint32_t dataType;
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t numBlocks;
   uint32_t testID;
} TISP_realImagToComplex_testParams_t;

void TISP_realImagToComplex_getTestParams(TISP_realImagToComplex_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_REALIMAGTOCOMPLEX_IXX_IXX_OXX_IDAT_H */
