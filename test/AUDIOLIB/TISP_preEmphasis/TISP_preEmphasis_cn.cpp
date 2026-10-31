// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_preEmphasis_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

float *getAlphaCoefficient();
float *getXPrev();

void AUDIOLIB_preEmphasis_cn_set(float alpha, size_t numChannels)
{
   float *alphaCoefficient = getAlphaCoefficient();
   float *xPrev            = getXPrev();

   *alphaCoefficient = alpha;
   for (size_t ch = 0; ch < numChannels; ch++) {
      xPrev[ch] = 0.0f;
   }
   return;
}

template <typename dataType>
void AUDIOLIB_preEmphasis_exec_cn(void  *pIn,
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
   float    *xPrev     = getXPrev();

   for (uint32_t i = 0; i < heightIn; i++) {
      dataType prev = xPrev[i];
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType x                   = pInLocal[i * strideIn + j];
         pOutLocal[i * strideOut + j] = x - alpha * prev;
         prev                         = x;
      }
      xPrev[i] = prev;
   }
   return;
}

template <typename dataType>
void AUDIOLIB_preEmphasisInterLeave_exec_cn(void  *pIn,
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
   float    *xPrev     = getXPrev();

   for (uint32_t i = 0; i < widthIn; ++i) {
      dataType prev = xPrev[i];
      for (uint32_t j = 0; j < heightIn; ++j) {
         dataType x                   = pInLocal[j * strideIn + i];
         pOutLocal[j * strideOut + i] = x - alpha * prev;
         prev                         = x;
      }
      xPrev[i] = prev;
   }
   return;
}

template void AUDIOLIB_preEmphasis_exec_cn<float>(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t strideIn,
                                                  size_t strideOut);
template void AUDIOLIB_preEmphasisInterLeave_exec_cn<float>(void  *pIn,
                                                            void  *pOut,
                                                            size_t widthIn,
                                                            size_t heightIn,
                                                            size_t strideIn,
                                                            size_t strideOut);
void          AUDIOLIB_preEmphasis_cn(void    *pIn,
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
         AUDIOLIB_preEmphasisInterLeave_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
      }
      else {
         AUDIOLIB_preEmphasis_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
      }
   }
}
