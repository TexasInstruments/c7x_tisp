// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include <dsplib.h>

// include test infrastructure provided by DSPLIB
#include "../common/TISP_test.h"

typedef struct {
   uint8_t  testPattern; /* 0: constant, 1: sequential, 2: random, 3: static array, 4: file, etc */
   void    *staticIn;
   void    *staticFilterCoeff;
   void    *staticOut;
   void    *staticFilterCoeffOpt;
   uint32_t dataType;
   uint32_t dataSize;
   uint32_t numChannels;
   uint32_t numStages;
   uint32_t biquadForm;
   uint32_t dataPitch;
   uint32_t filterVarPitch;
   uint32_t enableNChCoeffs;
   uint32_t enableCalcCoeffs;
   uint32_t bypassMode;         // 0 -> apply filter, 1 -> copy input to output
   uint32_t outputDataLocation; // 0 -> HEAP (probably L2SRAM), 1 -> MSMC
   uint32_t numReps;
   uint32_t testID;
} DSPLIB_cascadeBiquad_testParams_t;

void DSPLIB_cascadeBiquad_getTestParams(DSPLIB_cascadeBiquad_testParams_t **params, int32_t *numTests);
