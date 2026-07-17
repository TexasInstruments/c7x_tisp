// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_INTERLEAVE_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_INTERLEAVE_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <dsplib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   void    *staticIn;
   void    *staticOut;
   uint32_t dataType;
   uint32_t widthIn;
   uint32_t heightIn;
   uint32_t blockWidth;
   uint32_t blockHeight;
   int32_t  inPitch;
   int32_t  outPitch;
   uint32_t testID;
} TISP_interleave_superNode_testParams_t;

void TISP_interleave_superNode_getTestParams(TISP_interleave_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_INTERLEAVE_SUPERNODE_IXX_IXX_OXX_IDAT_H */
