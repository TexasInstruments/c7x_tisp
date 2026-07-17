// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_multiplexerSmooth_cn.h"
#include <audiolib.h>
#include <math.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_multiplexerSmooth_exec_cn(void   *pIn0,
                                        void   *pIn1,
                                        void   *pIn2,
                                        void   *pIn3,
                                        void   *pOut,
                                        size_t  width,
                                        size_t  height,
                                        size_t  inputs,
                                        size_t  strideInElements,
                                        size_t  strideOutElements,
                                        size_t *pPrevIdx,
                                        size_t *pCurrentIdx,
                                        size_t  smoothedSamples)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocalNext = (dataType *) pIn0;
   dataType *restrict pInLocalPrev = (dataType *) pIn1;
   dataType *restrict pGainCos     = (dataType *) pIn2;
   dataType *restrict pGainSine    = (dataType *) pIn3;
   dataType *restrict pOutLocalOut = (dataType *) pOut;

   if (*pCurrentIdx >= inputs) {
      *pCurrentIdx = *pPrevIdx;
   }

   dataType *restrict pInLocal =
       (dataType *) pInLocalNext + ((*pCurrentIdx * height * strideInElements)); // ACtual Buffer

   if ((*pPrevIdx == *pCurrentIdx) || (smoothedSamples == 0)) {
      for (uint32_t row = 0; row < height; row++) {
         for (uint32_t col = 0; col < width; col++) {
            /* Calculate source and destination indices using strides in elements */
            uint32_t srcIdx = row * strideInElements + col;
            uint32_t dstIdx = row * strideOutElements + col;

            /* Copy data */
            pOutLocalOut[dstIdx] = pInLocal[srcIdx];
         }
      }
   }
   else {

      if (width <= smoothedSamples) {
         /* Full frame to be faded */
         for (uint32_t i = 0; i < height; i++) {
            for (uint32_t j = 0; j < width; j++) {
               pOutLocalOut[i * width + j] =
                   pInLocal[i * width + j] * pGainSine[j] + pInLocalPrev[i * width + j] * pGainCos[j];
            }
         }
      }
      else {
         dataType *pOutL     = new dataType[smoothedSamples * height];
         dataType *pInSmooth = new dataType[smoothedSamples * height];

         /* Copy smoothedSamples samples from input buffer */
         for (size_t j = 0; j < height; j++) {
            for (size_t i = 0; i < smoothedSamples; i++) {
               pInSmooth[j * smoothedSamples + i] = pInLocal[j * width + i];
            }
         }

         /* Do fading */
         for (uint32_t i = 0; i < height; i++) {
            for (uint32_t j = 0; j < smoothedSamples; j++) {
               pOutL[i * smoothedSamples + j] = (pInSmooth[i * smoothedSamples + j] * pGainSine[j]) +
                                                (pInLocalPrev[i * smoothedSamples + j] * pGainCos[j]);
            }
         }

         /* Copy faded samples to actual output buffer */
         for (size_t j = 0; j < height; j++) {
            for (size_t i = 0; i < smoothedSamples; i++) {
               pOutLocalOut[j * width + i] = pOutL[j * smoothedSamples + i];
            }
         }

         /* Copy rest of the samples as actual input */
         for (size_t j = 0; j < height; j++) {
            for (size_t i = smoothedSamples; i < width; i++) {
               pOutLocalOut[j * width + i] = pInLocal[j * width + i];
            }
         }
         delete[] pInSmooth;
         pInSmooth = nullptr;
         delete[] pOutL;
         pOutL = nullptr;
      }
   }

   /* Copy new samples to the end of the buffer */
   for (size_t j = 0; j < height; j++) {
      for (size_t i = 0; i < smoothedSamples; i++) {
         pInLocalPrev[j * smoothedSamples + i] = pInLocal[j * width + i];
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_multiplexerSmooth_interleave_exec_cn(void   *pIn0,
                                                   void   *pIn1,
                                                   void   *pIn2,
                                                   void   *pIn3,
                                                   void   *pOut,
                                                   size_t  width,
                                                   size_t  height,
                                                   size_t  inputs,
                                                   size_t  strideInElements,
                                                   size_t  strideOutElements,
                                                   size_t *pPrevIdx,
                                                   size_t *pCurrentIdx,
                                                   size_t  smoothedSamples)
{
   /* Typecast void pointers to respective data type */

   dataType *restrict pInLocalNext = (dataType *) pIn0;
   dataType *restrict pInLocalPrev = (dataType *) pIn1;
   dataType *restrict pGainCos     = (dataType *) pIn2;
   dataType *restrict pGainSine    = (dataType *) pIn3;
   dataType *restrict pOutLocalOut = (dataType *) pOut;

   if (*pCurrentIdx >= inputs) {
      *pCurrentIdx = *pPrevIdx;
   }

   dataType *restrict pInLocal = (dataType *) pInLocalNext + ((*pCurrentIdx * height * strideInElements));

   if ((*pPrevIdx == *pCurrentIdx) || (smoothedSamples == 0)) {
      for (uint32_t row = 0; row < height; row++) {
         for (uint32_t col = 0; col < width; col++) {
            /* Calculate source and destination indices using strides in elements */
            uint32_t srcIdx = row * strideInElements + col;
            uint32_t dstIdx = row * strideOutElements + col;

            /* Copy data */
            pOutLocalOut[dstIdx] = pInLocal[srcIdx];
         }
      }
   }
   else {
      if (height <= smoothedSamples) {
         /* Full frame to be faded */
         for (uint32_t i = 0; i < height; i++) {
            for (uint32_t j = 0; j < width; j++) {
               pOutLocalOut[i * width + j] =
                   pInLocal[i * width + j] * pGainSine[i] + pInLocalPrev[i * width + j] * pGainCos[i];
            }
         }
      }
      else {
         dataType *pInput = new dataType[smoothedSamples * width];
         dataType *pOutL  = new dataType[smoothedSamples * width];

         /* Copy smoothedSamples samples from input buffer */
         for (size_t j = 0; j < smoothedSamples; j++) {
            for (size_t i = 0; i < width; i++) {
               pInput[j * width + i] = pInLocal[j * width + i];
            }
         }

         /* Do fading */
         for (uint32_t i = 0; i < smoothedSamples; i++) {
            for (uint32_t j = 0; j < width; j++) {
               pOutL[i * width + j] = pInput[i * width + j] * pGainSine[i] + pInLocalPrev[i * width + j] * pGainCos[i];
            }
         }

         /* Copy faded samples to actual output buffer */
         for (size_t j = 0; j < smoothedSamples; j++) {
            for (size_t i = 0; i < width; i++) {
               pOutLocalOut[j * width + i] = pOutL[j * width + i];
            }
         }

         /* Copy rest of the samples as actual input */
         for (size_t j = smoothedSamples; j < height; j++) {
            for (size_t i = 0; i < width; i++) {
               pOutLocalOut[j * width + i] = pInLocal[j * width + i];
            }
         }

         delete[] pInput;
         pInput = nullptr;
         delete[] pOutL;
         pOutL = nullptr;
      }
   }

   for (size_t j = 0; j < smoothedSamples; j++) {
      for (size_t i = 0; i < width; i++) {
         pInLocalPrev[j * width + i] = pInLocal[j * width + i];
      }
   }

   return;
}

template void AUDIOLIB_multiplexerSmooth_interleave_exec_cn<float>(void   *pIn0,
                                                                   void   *pIn1,
                                                                   void   *pIn2,
                                                                   void   *pIn3,
                                                                   void   *pOut,
                                                                   size_t  width,
                                                                   size_t  height,
                                                                   size_t  inputs,
                                                                   size_t  strideInElements,
                                                                   size_t  strideOutElements,
                                                                   size_t *pPrevIdx,
                                                                   size_t *pCurrentIdx,
                                                                   size_t  smoothedSamples);

template void AUDIOLIB_multiplexerSmooth_interleave_exec_cn<double>(void   *pIn0,
                                                                    void   *pIn1,
                                                                    void   *pIn2,
                                                                    void   *pIn3,
                                                                    void   *pOut,
                                                                    size_t  width,
                                                                    size_t  height,
                                                                    size_t  inputs,
                                                                    size_t  strideInElements,
                                                                    size_t  strideOutElements,
                                                                    size_t *pPrevIdx,
                                                                    size_t *pCurrentIdx,
                                                                    size_t  smoothedSamples);

template void AUDIOLIB_multiplexerSmooth_exec_cn<float>(void   *pIn0,
                                                        void   *pIn1,
                                                        void   *pIn2,
                                                        void   *pIn3,
                                                        void   *pOut,
                                                        size_t  width,
                                                        size_t  height,
                                                        size_t  inputs,
                                                        size_t  strideInElements,
                                                        size_t  strideOutElements,
                                                        size_t *pPrevIdx,
                                                        size_t *pCurrentIdx,
                                                        size_t  smoothedSamples);

template void AUDIOLIB_multiplexerSmooth_exec_cn<double>(void   *pIn0,
                                                         void   *pIn1,
                                                         void   *pIn2,
                                                         void   *pIn3,
                                                         void   *pOut,
                                                         size_t  width,
                                                         size_t  height,
                                                         size_t  inputs,
                                                         size_t  strideInElements,
                                                         size_t  strideOutElements,
                                                         size_t *pPrevIdx,
                                                         size_t *pCurrentIdx,
                                                         size_t  smoothedSamples);

void AUDIOLIB_multiplexerSmooth_cn(void    *pIn0,
                                   void    *pIn1,
                                   void    *pIn2,
                                   void    *pIn3,
                                   void    *pOut,
                                   size_t   width,
                                   size_t   height,
                                   size_t   inputs,
                                   size_t   strideInElements,
                                   size_t   strideOutElements,
                                   size_t  *pPrevIdx,
                                   size_t  *pCurrentIdx,
                                   size_t   smoothedSamples,
                                   size_t   isInterleave,
                                   uint32_t data_type)
{
   if (isInterleave == 1) {
      if (data_type == AUDIOLIB_FLOAT32) {
         AUDIOLIB_multiplexerSmooth_interleave_exec_cn<float>(pIn0, pIn1, pIn2, pIn3, pOut, width, height, inputs,
                                                              strideInElements, strideOutElements, pPrevIdx,
                                                              pCurrentIdx, smoothedSamples);
      }
      else if (data_type == AUDIOLIB_FLOAT64) {
         AUDIOLIB_multiplexerSmooth_interleave_exec_cn<double>(pIn0, pIn1, pIn2, pIn3, pOut, width, height, inputs,
                                                               strideInElements, strideOutElements, pPrevIdx,
                                                               pCurrentIdx, smoothedSamples);
      }
   }
   else {
      if (data_type == AUDIOLIB_FLOAT32) {
         AUDIOLIB_multiplexerSmooth_exec_cn<float>(pIn0, pIn1, pIn2, pIn3, pOut, width, height, inputs,
                                                   strideInElements, strideOutElements, pPrevIdx, pCurrentIdx,
                                                   smoothedSamples);
      }
      else if (data_type == AUDIOLIB_FLOAT64) {
         AUDIOLIB_multiplexerSmooth_exec_cn<double>(pIn0, pIn1, pIn2, pIn3, pOut, width, height, inputs,
                                                    strideInElements, strideOutElements, pPrevIdx, pCurrentIdx,
                                                    smoothedSamples);
      }
   }
}
