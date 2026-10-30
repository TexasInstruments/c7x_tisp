// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_NONZEROCHECK_SUPERNODE_IXX_OXX_IDAT_H

#define TISP_NONZEROCHECK_SUPERNODE_IXX_OXX_IDAT_H

#include <dsplib.h>

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
   uint32_t blockWidth;
   uint32_t blockHeight;
   uint32_t isInterleave;
   uint32_t opType;   /* 0: ALL, 1: ANY */
   uint32_t axisType; /* 0: ROW, 1: COLUMN */
   int32_t  inPitch;
   int32_t  outPitch;
   uint32_t testID;
} TISP_nonZeroCheck_superNode_testParams_t;

void TISP_nonZeroCheck_superNode_getTestParams(TISP_nonZeroCheck_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_NONZEROCHECK_SUPERNODE_IXX_OXX_IDAT_H */
