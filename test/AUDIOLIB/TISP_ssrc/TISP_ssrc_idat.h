// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_SSRC_IXX_IXX_OXX_IDAT_H

#define TISP_SSRC_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

/* include test infrastructure provided by MMALIB */
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t testPattern; /* 0: constant, 1: sequential, 2: random, 3: static
                           array, 4: file, etc */
   void    *staticIn;
   void    *staticOut;
   uint32_t sampleDataType;
   uint32_t inputSampleCount;
   uint8_t  inputSampleRate;
   uint8_t  outputSampleRate;
   uint8_t  numChannels;
   uint32_t numBlocks;
   float    signalFrequency;
   uint8_t  enableMMA; /* 1 -> enable MMA, 0 -> Disable MMA or if the SoC does not support MMA */
   uint8_t  dataFormat;
   uint32_t outputDataLocation; /* 0 -> HEAP (probably L2SRAM), 1 -> MSMC */
   uint32_t numReps;
   uint32_t testCategory;
   uint32_t testID;

} TISP_ssrc_testParams_t;

void TISP_ssrc_getTestParams(TISP_ssrc_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_SSRC_IXX_IXX_OXX_IDAT_H */
