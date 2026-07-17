// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_MIXER_IXX_IXX_OXX_IDAT_H

#define TISP_MIXER_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern;       /* 0: constant, 1: sequential, 2: random, 3: static array, 4: file, etc */
   void   **staticIn;          // Pointer to an array of input buffer pointers
   void    *staticOut;         // Output buffer pointer
   void    *staticMixerCoeffs; // Mixer coefficients
   int32_t *inChannels;        // Number of channels for each input
   uint32_t dataType;          // Precision (e.g., AUDIOLIB_FLOAT32)
   uint32_t inSamples;         // Number of samples
   uint32_t totalInChannels;   // Total number of channels across all inputs
   uint32_t numInput;          // Number of inputs
   uint32_t numOutputChannels;
   uint8_t  isInterleave;       // Flag for input interleaving
   uint32_t strideIn0;          // Stride for input (aligned to 64 bytes)
   uint32_t strideOut;          // Stride for output (aligned to 64 bytes)
   uint32_t outputDataLocation; // 0 -> HEAP (L2SRAM), 1 -> MSMC
   uint32_t numReps;            // Number of repetitions
   uint32_t numBlocks;          // Number of blocks
   uint32_t testID;             // Test ID
} TISP_mixer_testParams_t;

void TISP_mixer_getTestParams(TISP_mixer_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_MIXER_IXX_IXX_OXX_IDAT_H */
