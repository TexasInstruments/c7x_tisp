// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_FIR_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_FIR_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <dsplib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t testPattern; /* 0: constant, 1: sequential, 2: random, 3: static
                           array, 4: file, etc */
   const void *staticIn;
   const void *filter;
   const void *staticOut;
   uint32_t    dataType;
   uint32_t    dataSize;
   uint32_t    numBlocks;
   uint32_t    dataPitchIn;
   uint32_t    dataPitchOut;
   uint32_t    batchSize;
   uint32_t    filterSize;
   uint32_t    filterPitch;
   uint32_t    enableNchCoefs;
   uint32_t    enableMMA;
   uint32_t    enableQ;
   uint32_t    Q;
   uint32_t    outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t    numReps;
   uint32_t    testID;
} TISP_fir_superNode_testParams_t;

void TISP_fir_superNode_getTestParams(TISP_fir_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_FIR_SUPERNODE_IXX_IXX_OXX_IDAT_H */
