// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef DSPLIB_FIR_IXX_IXX_OXX_IDAT_H

#define DSPLIB_FIR_IXX_IXX_OXX_IDAT_H

#include <dsplib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t testPattern; /* 0: constant, 1: sequential, 2: random, 3: static
                           array, 4: file, etc */
   void    *staticIn;
   void    *filter;
   void    *staticOut;
   uint32_t dataType;
   uint32_t dataSize;
   uint32_t numBlocks;
   uint32_t dataPitchIn;
   uint32_t dataPitchOut;
   uint32_t batchSize;
   uint32_t filterSize;
   uint32_t filterPitch;
   uint32_t enableNchCoefs;
   uint32_t enableMMA;
   uint32_t enableQ;
   uint32_t Q;
   uint32_t bypassMode;         // 0 -> apply filter, 1 -> copy input to output
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t testID;
} DSPLIB_fir_testParams_t;

void DSPLIB_fir_getTestParams(DSPLIB_fir_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define DSPLIB_FIR_IXX_IXX_OXX_IDAT_H */
