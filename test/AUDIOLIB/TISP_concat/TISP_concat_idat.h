// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_CONCAT_IXX_IXX_OXX_IDAT_H

#define TISP_CONCAT_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern;        // 0=constant, 1=sequential, 2=random, STATIC=static array
   void   **staticIn;           // pointer to array of numInputs input data pointers
   void    *staticOut;          // pointer to expected output data
   uint32_t inSamples;          // samples per channel per input
   uint32_t totalInChannels;    // total channels across all inputs (sum of inChannels)
   uint32_t *inChannels;        // pointer to array of per-input channel counts (one per input)
   uint8_t  isInterleave;       // 0=planar, 1=interleaved
   uint32_t numInputs;          // number of input buffers
   uint32_t strideIn;           // stride for each input buffer in bytes
   uint32_t strideOut;          // stride for output buffer in bytes
   uint32_t dataType;           // AUDIOLIB_FLOAT32 or AUDIOLIB_FLOAT64
   uint32_t outputDataLocation; // AUDIOLIB_TEST_OUTPUT_MSMC etc.
   uint32_t numReps;
   uint32_t testID;
} TISP_concat_testParams_t;

void TISP_concat_getTestParams(TISP_concat_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_CONCAT_IXX_IXX_OXX_IDAT_H */

/* ======================================================================== */
/*  End of file:  TISP_concat_idat.h                                     */
/* ======================================================================== */
