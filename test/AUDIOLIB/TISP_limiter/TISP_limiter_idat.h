// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_LIMITER_IDAT_H
#define TISP_LIMITER_IDAT_H

#include <audiolib.h>

#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {

   uint32_t testId;
   uint32_t testPattern;
   uint32_t outputDataLocation;
   uint32_t numReps;
   uint32_t numChannels;
   uint32_t blockSize;
   uint32_t dataType;
   uint32_t pitchX;
   uint32_t pitchS;
   uint32_t pitchY;
   uint32_t isInterleaved;
   uint32_t bypassMode;
   void*    staticIn;
   void*    staticOut;
   void*    staticGainOut;
   void*    threshold_dB;
   void*    kneeWidth_dB;
   void*    attackCoeff;
   void*    releaseCoeff;

} TISP_limiter_testParams_t;

void TISP_limiter_getTestParams(TISP_limiter_testParams_t** params, int32_t* numTests);

#ifdef __cplusplus
}
#endif
#endif /* define TISP_LIMITER_IDAT_H */
