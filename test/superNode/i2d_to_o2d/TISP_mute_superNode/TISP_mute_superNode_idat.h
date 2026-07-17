// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_MUTE_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_MUTE_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

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
   uint32_t isMute;
   int32_t  samplingRate;
   float    fadeTime;
   uint8_t  isInterleaved;
   uint32_t fadeType; // 0=LINEAR, 1=SMOOTH, 2=HARD
   uint32_t inPitch;
   uint32_t outPitch;
   uint32_t blockWidth;
   uint32_t blockHeight;
   uint32_t testID;
} TISP_mute_superNode_testParams_t;

void TISP_mute_superNode_getTestParams(TISP_mute_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_MUTE_SUPERNODE_IXX_IXX_OXX_IDAT_H */
