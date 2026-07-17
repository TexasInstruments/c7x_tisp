// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_ASRC_IXX_IXX_OXX_IDAT_H

#define TISP_ASRC_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

/*  include test infrastructure provided by MMALIB */
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern; /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc.*/
   void    *staticIn0;
   void    *staticOut;
   uint32_t sampleDataType;
   uint32_t maxSampleCountPerBlock;
   uint32_t inputSampleRate;
   uint32_t outputSampleRate;
   uint32_t heightIn;
   uint32_t numBlocks;
   float    signalFrequency;
   uint32_t genOutSamplesPerChannel;
   uint32_t moduloFactor;
   uint32_t dataFormat;
   uint32_t outputDataLocation; /* 0 -> HEAP (probably L2SRAM), 1 -> MSMC */
   uint32_t numReps;
   uint32_t testID;

} TISP_asrc_testParams_t;

void TISP_asrc_getTestParams(TISP_asrc_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_ASRC_IXX_IXX_OXX_IDAT_H */
