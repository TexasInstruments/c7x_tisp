// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_mixer_cn.h"
#include "common/AUDIOLIB_bufParams.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

/* ------------------------------------------------------------------- */
/* 1. Input Aggregator CN                                             */
/* ------------------------------------------------------------------- */
template <typename dataType>
void AUDIOLIB_inputAggregator_exec_cn(void **RESTRICT pIn,
                                      void *RESTRICT  pOut,
                                      int32_t         numInputs,
                                      int32_t        *pInChannels,
                                      int32_t         numSamples,
                                      int32_t         strideInElements,
                                      int32_t         strideOutElements, // Stride of Scratch (Planar)
                                      int32_t         isInputInterleave)
{
   dataType *RESTRICT pOutLocal        = static_cast<dataType *>(pOut);
   uint32_t           outChannelOffset = 0;

   // Always output Planar to Scratch (isOutputInterleave = 0)
   for (int32_t i = 0; i < numInputs; i++) {
      dataType *RESTRICT pInLocal    = static_cast<dataType *>(pIn[i]);
      int32_t            numChannels = pInChannels[i];

      if (isInputInterleave) {
         // Input: Interleaved (Samples x Channels)
         // Output: Planar (Channels x Samples) to Scratch
         for (int32_t c = 0; c < numChannels; c++) {
            for (int32_t s = 0; s < numSamples; s++) {
               // In: Row=s, Col=c, Stride=strideIn (pitch of one sample set)
               // Out: Row=c+offset, Col=s, Stride=strideOut (pitch of one channel)
               pOutLocal[(outChannelOffset + c) * strideOutElements + s] = pInLocal[s * strideInElements + c];
            }
         }
      }
      else {
         // Input: Planar (Channels x Samples)
         // Output: Planar (Channels x Samples) to Scratch
         // Since input is effectively a list of pointers to single channels (or blocks),
         // we treat pInLocal as a block of [Channels * Samples]

         for (int32_t c = 0; c < numChannels; c++) {
            for (int32_t s = 0; s < numSamples; s++) {
               pOutLocal[(outChannelOffset + c) * strideOutElements + s] = pInLocal[c * strideInElements + s];
            }
         }
      }
      outChannelOffset += numChannels;
   }
}

/* ------------------------------------------------------------------- */
/* 2. Matrix Multiply CN                                              */
/* ------------------------------------------------------------------- */
template <typename dataType>
void DSPLIB_matMul_exec_cn(void *RESTRICT pIn0,    // A: Coeffs
                           void *RESTRICT pIn1,    // B: Scratch (Input)
                           void *RESTRICT pOut,    // C: Output
                           int32_t        M,       // Rows of A / Rows of C (Num Output Ch)
                           int32_t        K,       // Cols of A / Rows of B (Total Input Ch)
                           int32_t        N,       // Cols of B / Cols of C (Num Samples)
                           int32_t        strideA, // Stride of Coeffs
                           int32_t        strideB, // Stride of Scratch
                           int32_t        strideC)        // Stride of Output
{
   const dataType *RESTRICT A = static_cast<const dataType *>(pIn0);
   const dataType *RESTRICT B = static_cast<const dataType *>(pIn1);
   dataType *RESTRICT       C = static_cast<dataType *>(pOut);

   for (int32_t m = 0; m < M; m++) {    // For every Output Channel
      for (int32_t n = 0; n < N; n++) { // For every Sample
         dataType sum = 0;
         for (int32_t k = 0; k < K; k++) { // Dot Product over Input Channels
            // A[m, k] * B[k, n]
            sum += A[m * strideA + k] * B[k * strideB + n];
         }
         C[m * strideC + n] = sum;
      }
   }
}

/* ------------------------------------------------------------------- */
/* 3. Interleaver CN (Transpose)                                      */
/* ------------------------------------------------------------------- */
template <typename dataType>
void DSPLIB_interleave_exec_cn(void *RESTRICT pIn,  // Planar Input
                               void *RESTRICT pOut, // Interleaved Output
                               int32_t        rows, // Input Rows (Channels)
                               int32_t        cols, // Input Cols (Samples)
                               int32_t        strideIn,
                               int32_t        strideOut)
{
   dataType *src = static_cast<dataType *>(pIn);
   dataType *dst = static_cast<dataType *>(pOut);

   for (int32_t r = 0; r < rows; r++) {
      for (int32_t c = 0; c < cols; c++) {
         dst[c * strideOut + r] = src[r * strideIn + c];
      }
   }
}

template void AUDIOLIB_inputAggregator_exec_cn<float>(void **RESTRICT pIn,
                                                      void *RESTRICT  pOut,
                                                      int32_t         numInputs,
                                                      int32_t        *pInChannels,
                                                      int32_t         numSamples,
                                                      int32_t         strideInElements,
                                                      int32_t         strideOutElements, // Stride of Scratch (Planar)
                                                      int32_t         isInputInterleave);

template void DSPLIB_matMul_exec_cn<float>(void *RESTRICT pIn0,    // A: Coeffs
                                           void *RESTRICT pIn1,    // B: Scratch (Input)
                                           void *RESTRICT pOut,    // C: Output
                                           int32_t        M,       // Rows of A / Rows of C (Num Output Ch)
                                           int32_t        K,       // Cols of A / Rows of B (Total Input Ch)
                                           int32_t        N,       // Cols of B / Cols of C (Num Samples)
                                           int32_t        strideA, // Stride of Coeffs
                                           int32_t        strideB, // Stride of Scratch
                                           int32_t        strideC);

template void DSPLIB_interleave_exec_cn<float>(void *RESTRICT pIn,  // Planar Input
                                               void *RESTRICT pOut, // Interleaved Output
                                               int32_t        rows, // Input Rows (Channels)
                                               int32_t        cols, // Input Cols (Samples)
                                               int32_t        strideIn,
                                               int32_t        strideOut);

template void AUDIOLIB_inputAggregator_exec_cn<double>(void **RESTRICT pIn,
                                                       void *RESTRICT  pOut,
                                                       int32_t         numInputs,
                                                       int32_t        *pInChannels,
                                                       int32_t         numSamples,
                                                       int32_t         strideInElements,
                                                       int32_t         strideOutElements, // Stride of Scratch (Planar)
                                                       int32_t         isInputInterleave);

template void DSPLIB_matMul_exec_cn<double>(void *RESTRICT pIn0,    // A: Coeffs
                                            void *RESTRICT pIn1,    // B: Scratch (Input)
                                            void *RESTRICT pOut,    // C: Output
                                            int32_t        M,       // Rows of A / Rows of C (Num Output Ch)
                                            int32_t        K,       // Cols of A / Rows of B (Total Input Ch)
                                            int32_t        N,       // Cols of B / Cols of C (Num Samples)
                                            int32_t        strideA, // Stride of Coeffs
                                            int32_t        strideB, // Stride of Scratch
                                            int32_t        strideC);

template void DSPLIB_interleave_exec_cn<double>(void *RESTRICT pIn,  // Planar Input
                                                void *RESTRICT pOut, // Interleaved Output
                                                int32_t        rows, // Input Rows (Channels)
                                                int32_t        cols, // Input Cols (Samples)
                                                int32_t        strideIn,
                                                int32_t        strideOut);

/* ------------------------------------------------------------------- */
/* Top Level Mixer CN                                                 */
/* ------------------------------------------------------------------- */
void AUDIOLIB_mixer_cn(void   **pIn,
                       void    *pOut,
                       void    *pCoeffs,
                       void    *pScratch,
                       void    *pTempOut,
                       int32_t  numInputs,
                       int32_t *pInChannels,
                       int32_t  numOutputChannels,
                       int32_t  numSamples,
                       int32_t  totalInChannels,
                       int32_t  strideInElements,
                       int32_t  strideOutElements,
                       int32_t  strideScratch,
                       int32_t  strideTempOut,
                       int32_t  isInterleave,
                       int32_t  strideCoeff,
                       int32_t  dataType)
{
   if (dataType == AUDIOLIB_FLOAT32) {
      // 1. Aggregator (Inputs -> Planar Scratch)
      AUDIOLIB_inputAggregator_exec_cn<float>(pIn, pScratch, numInputs, pInChannels, numSamples, strideInElements,
                                              strideScratch, // Use passed stride
                                              isInterleave);

      if (isInterleave) {
         // 2. MatMul (Coeffs * Scratch -> Planar TempOut)
         DSPLIB_matMul_exec_cn<float>(pCoeffs, pScratch, pTempOut, numOutputChannels, totalInChannels, numSamples,
                                      strideCoeff,   // Stride A (Coeffs width)
                                      strideScratch, // Stride B
                                      strideTempOut  // Stride C
         );

         // 3. Interleave (Planar TempOut -> Interleaved pOut)
         DSPLIB_interleave_exec_cn<float>(pTempOut, pOut, numOutputChannels, numSamples, strideTempOut,
                                          strideOutElements);
      }
      else {
         // Optimization: Direct MatMul to pOut
         DSPLIB_matMul_exec_cn<float>(pCoeffs, pScratch, pOut, numOutputChannels, totalInChannels, numSamples,
                                      strideCoeff, strideScratch, strideOutElements);
      }
   }
   else if (dataType == AUDIOLIB_FLOAT64) {
      // Double precision implementation
      AUDIOLIB_inputAggregator_exec_cn<double>(pIn, pScratch, numInputs, pInChannels, numSamples, strideInElements,
                                               strideScratch, isInterleave);

      if (isInterleave) {
         DSPLIB_matMul_exec_cn<double>(pCoeffs, pScratch, pTempOut, numOutputChannels, totalInChannels, numSamples,
                                       strideCoeff, strideScratch, strideTempOut);

         DSPLIB_interleave_exec_cn<double>(pTempOut, pOut, numOutputChannels, numSamples, strideTempOut,
                                           strideOutElements);
      }
      else {
         DSPLIB_matMul_exec_cn<double>(pCoeffs, pScratch, pOut, numOutputChannels, totalInChannels, numSamples,
                                       strideCoeff, strideScratch, strideOutElements);
      }
   }
}
