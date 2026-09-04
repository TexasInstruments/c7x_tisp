// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: Apache-2.0

#ifndef TISP_LOGMELSPECTRUM_IDAT_H
#define TISP_LOGMELSPECTRUM_IDAT_H

#include <stdint.h>

#include <audiolib.h>

#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOTE: the member order below MUST match the write order in
 * test/TISP_idat_gen/AUDIOLIB/TISP_logMelSpectrum/gen_data.py
 * (gen_testCaseParams).  If you add/remove a field, update both.
 */
typedef struct {
   uint8_t  testPattern;    /* 0: constant, 1: sequential, 2: sequential_negative, 3: random etc. */
   float   *staticIn;       /* input waveform [numChannels x numInputSamples] */
   float   *staticOut;      /* expected log-mel [numChannels x numWindows x numMelBins], or NULL */
   uint32_t fftSize;
   uint32_t windowLength;
   uint32_t hopSize;
   uint32_t numChannels;
   uint32_t numInputSamples;
   uint32_t numMelBins;
   uint32_t sampleRate;
   float    fMin;           /* lowest mel edge (Hz) */
   float    fMax;           /* highest mel edge (Hz); None resolved to sampleRate/2 in gen */
   float    power;          /* magnitude exponent (1.0 = magnitude, 2.0 = power) */
   float    logOffset;      /* offset added before log */
   uint32_t melScale;       /* 0 = htk, 1 = slaney */
   uint32_t melNorm;        /* 0 = none, 1 = slaney area normalisation */
   uint32_t windowType;     /* TISP::WindowType enum (0=rect,1=hann,2=hamming,3=blackman,4=blackman_harris,5=bartlett,6=sqrt(hann)) */
   uint32_t windowAlign;    /* 0 = center-padded, 1 = left-aligned */
   uint32_t windowPeriodic; /* 1 = periodic (DFT-even), 0 = symmetric */
   uint32_t outputDataLocation; /* 0 -> HEAP (probably L2SRAM), 1 -> MSMC */
   uint32_t numReps;
   uint32_t testID;
   uint32_t numBlocks;

} TISP_logMelSpectrum_testParams_t;

void TISP_logMelSpectrum_getTestParams(TISP_logMelSpectrum_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* TISP_LOGMELSPECTRUM_IDAT_H */

/* ======================================================================== */
/*  End of file:  TISP_logMelSpectrum_idat.h                                */
/* ======================================================================== */
