// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_CONDITIONALREPLACE_SUPERNODE_IXX_OXX_IDAT_H

#define TISP_CONDITIONALREPLACE_SUPERNODE_IXX_OXX_IDAT_H

#include <dsplib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern;
   void    *staticIn0;
   void    *staticIn1; /* per-channel arrays: compareType, constValue, replaceValue (heightIn each) */
   void    *staticOut;
   uint32_t dataType;
   uint32_t widthIn;
   uint32_t heightIn;
   uint32_t blockWidth;
   uint32_t blockHeight;
   uint32_t isInterleave;
   int32_t  inPitch;
   int32_t  outPitch;
   uint32_t testID;
} TISP_conditionalReplace_superNode_testParams_t;

void TISP_conditionalReplace_superNode_getTestParams(TISP_conditionalReplace_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_CONDITIONALREPLACE_SUPERNODE_IXX_OXX_IDAT_H */
