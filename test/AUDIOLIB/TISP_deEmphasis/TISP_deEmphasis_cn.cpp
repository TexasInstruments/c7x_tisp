// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_deEmphasis_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

float *getAlphaCoefficient();
float *getYPrev();

void AUDIOLIB_deEmphasis_cn_set(float alpha, size_t numChannels)
{
   float *alphaCoefficient = getAlphaCoefficient();
   float *yPrev            = getYPrev();

   *alphaCoefficient = alpha;
   for (size_t ch = 0; ch < numChannels; ch++) {
      yPrev[ch] = 0.0f;
   }
   return;
}

template <typename dataType>
void AUDIOLIB_deEmphasis_exec_cn(void  *pIn,
                                 void  *pOut,
                                 size_t widthIn,
                                 size_t heightIn,
                                 size_t strideIn,
                                 size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;
   float     alpha     = *getAlphaCoefficient();
   float    *yPrev     = getYPrev();

   for (uint32_t i = 0; i < heightIn; i++) {
      dataType prev = yPrev[i];
      for (uint32_t j = 0; j < widthIn; j++) {
         prev                         = pInLocal[i * strideIn + j] + alpha * prev;
         pOutLocal[i * strideOut + j] = prev;
      }
      yPrev[i] = prev;
   }
   return;
}

template <typename dataType>
void AUDIOLIB_deEmphasisInterLeave_exec_cn(void  *pIn,
                                           void  *pOut,
                                           size_t widthIn,
                                           size_t heightIn,
                                           size_t strideIn,
                                           size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;
   float     alpha     = *getAlphaCoefficient();
   float    *yPrev     = getYPrev();

   for (uint32_t i = 0; i < widthIn; ++i) {
      dataType prev = yPrev[i];
      for (uint32_t j = 0; j < heightIn; ++j) {
         prev                         = pInLocal[j * strideIn + i] + alpha * prev;
         pOutLocal[j * strideOut + i] = prev;
      }
      yPrev[i] = prev;
   }
   return;
}

template void AUDIOLIB_deEmphasis_exec_cn<float>(void  *pIn,
                                                 void  *pOut,
                                                 size_t widthIn,
                                                 size_t heightIn,
                                                 size_t strideIn,
                                                 size_t strideOut);
template void AUDIOLIB_deEmphasisInterLeave_exec_cn<float>(void  *pIn,
                                                           void  *pOut,
                                                           size_t widthIn,
                                                           size_t heightIn,
                                                           size_t strideIn,
                                                           size_t strideOut);
void          AUDIOLIB_deEmphasis_cn(void    *pIn,
                                     void    *pOut,
                                     uint32_t data_type,
                                     size_t   widthIn,
                                     size_t   heightIn,
                                     size_t   strideIn,
                                     size_t   strideOut,
                                     size_t   isInterleave)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      if (isInterleave == 1) {
         AUDIOLIB_deEmphasisInterLeave_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
      }
      else {
         AUDIOLIB_deEmphasis_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
      }
   }
}
