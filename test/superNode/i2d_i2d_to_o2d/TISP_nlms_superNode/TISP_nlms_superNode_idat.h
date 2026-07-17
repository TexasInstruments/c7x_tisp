// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_NLMS_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_NLMS_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t testPattern; /* 0: constant, 1: sequential, 2: random, 3: static
                           array, 4: file, etc */
   void    *staticIn0;
   void    *staticDesired;
   void    *staticOut;
   void    *staticCoefficient;
   uint32_t dataType;
   uint32_t inSamples;
   uint32_t inChannels;
   uint32_t isInterleave;
   uint32_t strideIn;
   uint32_t strideOut;
   uint32_t filterLength;
   float    stepSize;
   uint32_t blockWidth;
   uint32_t blockHeight;
   uint32_t numBlocks;
   uint32_t testID;
} TISP_nlms_superNode_testParams_t;

void TISP_nlms_superNode_getTestParams(TISP_nlms_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_NLMS_SUPERNODE_IXX_IXX_OXX_IDAT_H */
