// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_DELAY_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_DELAY_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern;
   uint32_t delaySize;
   uint32_t dataType;
   int32_t  mode;
   void    *staticIn0;
   void    *staticOut;
   void    *staticDelay;
   int32_t  interleave;
   uint32_t heightIn;
   uint32_t widthIn;
   int32_t  inPitch;
   int32_t  outPitch;
   uint32_t blockWidth;
   uint32_t blockHeight;
   uint32_t testID;
} TISP_delay_superNode_testParams_t;

void TISP_delay_superNode_getTestParams(TISP_delay_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_DELAY_SUPERNODE_IXX_IXX_OXX_IDAT_H */
