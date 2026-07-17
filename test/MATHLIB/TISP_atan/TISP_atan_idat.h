// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef MATHLIB_ATAN_IXX_IXX_OXX_IDAT_H

#define MATHLIB_ATAN_IXX_IXX_OXX_IDAT_H

#include <mathlib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern; /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc.*/
   void    *staticIn;
   void    *staticOut;
   uint32_t dataType;
   uint32_t length;
   uint32_t numReps;
   uint32_t numBlocks;
   uint32_t testID;
} TISP_atan_testParams_t;

void TISP_atan_getTestParams(TISP_atan_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define MATHLIB_ATAN_IXX_IXX_OXX_IDAT_H */
