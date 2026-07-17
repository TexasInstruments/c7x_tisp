// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_MATMUL_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_MATMUL_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <dsplib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t testPattern; /* 0: constant, 1: sequential, 2: random, 3: static
                           array, 4: file, etc */
   const void *staticInA;
   const void *staticInB;
   const void *staticOut;
   // char* dataPathA;
   // char* dataPathB;
   // char* dataPathOut;
   uint32_t dataType;
   uint32_t m;
   uint32_t n;
   uint32_t k;
   uint32_t dataPitchInA;
   uint32_t dataPitchInB;
   uint32_t dataPitchOut;
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t testID;
} TISP_matMul_superNode_testParams_t;

void TISP_matMul_superNode_getTestParams(TISP_matMul_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_matMul_superNode_IXX_IXX_OXX_IDAT_H */
