// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_ISTFT_IDAT_H
#define TISP_ISTFT_IDAT_H

#include <stdint.h>

#include <audiolib.h>

#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern;    /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc. */
   float   *staticIn;       /* complex input [numBlocks x numChannels x numWindows x inBins] */
   float   *staticOut;      /* expected output [numBlocks x numChannels x numWindows x hopSize], or NULL */
   uint32_t fftSize;
   uint32_t windowLength;   /* logical synthesis-window length (<= fftSize); center-padded to fftSize */
   uint32_t hopSize;
   uint32_t numChannels;
   uint32_t numWindows;     /* spectral frames per exec() call */
   uint32_t onesided;       /* 1 = half spectrum (N/2+1 bins), 0 = full spectrum */
   uint32_t windowType;     /* TISP::WindowType enum value (0=rect, 1=hann, 2=hamming, ..., 6=sqrt_hann) */
   uint32_t windowAlign;    /* window placement in fftSize buffer: 0=center-padded, 1=left-aligned (right-zero-pad) */
   uint32_t windowPeriodic; /* window symmetry: 1=periodic (DFT-even, denom N), 0=symmetric (denom N-1) */
   uint32_t outputDataLocation; /* 0 -> HEAP (probably L2SRAM), 1 -> MSMC */
   uint32_t numReps;
   uint32_t testID;
   uint32_t numBlocks;      /* number of exec() calls per test run */

} TISP_istft_testParams_t;

void TISP_istft_getTestParams(TISP_istft_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* TISP_ISTFT_IDAT_H */
