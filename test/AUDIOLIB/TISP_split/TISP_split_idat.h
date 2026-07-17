// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_SPLIT_IXX_IXX_OXX_IDAT_H

#define TISP_SPLIT_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by MMALIB
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t   testPattern;        /* 0: constant, 1: sequential, 2: random, 3: static array, 4: file */
   void     *staticIn;           /* Single input buffer pointer  [S, C_total]                       */
   void    **staticOut;          /* Array of N output buffer pointers, each [S, C_i]                */
   uint32_t  inSamples;          /* Number of samples (S)                                           */
   uint32_t  totalInputChannels; /* Total channels in the input = sum of all output channels (C)    */
   uint32_t *outChannels;        /* Pointer to array of per-output channel counts (one per output)  */
   uint32_t  numOutputs;         /* Number of output ports (N)                                      */
   uint32_t  strideIn;           /* Stride for input  (aligned to 64 bytes)                        */
   uint32_t  strideOut;          /* Stride for output (aligned to 64 bytes)                        */
   uint32_t  dataType;           /* Precision (e.g., AUDIOLIB_FLOAT32)                             */
   uint32_t  outputDataLocation; /* 0 -> HEAP (L2SRAM), 1 -> MSMC  — mirrors inputAggregator       */
   uint32_t  numReps;            /* Number of repetitions                                           */
   uint32_t  testID;             /* Test ID                                                         */
   uint32_t  isInputInterleave;  /* 1: interleaved, 0: deinterleaved                              */
} TISP_split_testParams_t;

void TISP_split_getTestParams(TISP_split_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_SPLIT_IXX_IXX_OXX_IDAT_H */

/* ======================================================================== */
/*  End of file:  TISP_split_idat.h                                     */
/* ======================================================================== */
