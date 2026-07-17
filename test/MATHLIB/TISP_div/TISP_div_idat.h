// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef MATHLIB_DIV_IXX_IXX_OXX_IDAT_H

#define MATHLIB_DIV_IXX_IXX_OXX_IDAT_H

#include <mathlib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern; /* 0: const, 1: sequential, 2: sequential_negative, 3: random etc.*/
   void    *staticIn0;
   void    *staticIn1;
   void    *staticOut;
   uint32_t dataType;
   uint32_t length;
   uint32_t numReps;
   uint32_t numBlocks;
   uint32_t testID;
} TISP_div_testParams_t;

void TISP_div_getTestParams(TISP_div_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define MATHLIB_DIV_IXX_IXX_OXX_IDAT_H */
