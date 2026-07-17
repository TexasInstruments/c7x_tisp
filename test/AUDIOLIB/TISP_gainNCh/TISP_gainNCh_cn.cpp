// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_gainNCh_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_gainNCh_exec_cn(void  *pIn,
                              void  *pGain,
                              void  *pOut,
                              size_t widthIn,
                              size_t heightIn,
                              size_t strideIn,
                              size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal   = (dataType *) pIn;
   dataType *pGainLocal = (dataType *) pGain;
   dataType *pOutLocal  = (dataType *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      dataType gainCh = pGainLocal[i];
      for (uint32_t j = 0; j < widthIn; j++) {
         pOutLocal[i * strideOut + j] = pInLocal[i * strideIn + j] * gainCh;
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_gainNChInterLeave_exec_cn(void  *pIn,
                                        void  *pGain,
                                        void  *pOut,
                                        size_t widthIn,
                                        size_t heightIn,
                                        size_t strideIn,
                                        size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal   = (dataType *) pIn;
   dataType *pGainLocal = (dataType *) pGain;
   dataType *pOutLocal  = (dataType *) pOut;

   for (uint32_t i = 0; i < widthIn; ++i) {
      dataType gainCh = pGainLocal[i];
      for (uint32_t j = 0; j < heightIn; ++j) {
         pOutLocal[j * strideOut + i] = pInLocal[j * strideIn + i] * gainCh;
      }
   }
   return;
}

template void AUDIOLIB_gainNCh_exec_cn<float>(void  *pIn,
                                              void  *pGain,
                                              void  *pOut,
                                              size_t widthIn,
                                              size_t heightIn,
                                              size_t strideIn,
                                              size_t strideOut);
template void AUDIOLIB_gainNCh_exec_cn<double>(void  *pIn,
                                               void  *pGain,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideIn,
                                               size_t strideOut);
template void AUDIOLIB_gainNChInterLeave_exec_cn<float>(void  *pIn,
                                                        void  *pGain,
                                                        void  *pOut,
                                                        size_t widthIn,
                                                        size_t heightIn,
                                                        size_t strideIn,
                                                        size_t strideOut);
template void AUDIOLIB_gainNChInterLeave_exec_cn<double>(void  *pIn,
                                                         void  *pGain,
                                                         void  *pOut,
                                                         size_t widthIn,
                                                         size_t heightIn,
                                                         size_t strideIn,
                                                         size_t strideOut);
void          AUDIOLIB_gainNCh_cn(void    *pIn,
                                  void    *pGain,
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
         AUDIOLIB_gainNChInterLeave_exec_cn<float>(pIn, pGain, pOut, widthIn, heightIn, strideIn, strideOut);
      }
      else {
         AUDIOLIB_gainNCh_exec_cn<float>(pIn, pGain, pOut, widthIn, heightIn, strideIn, strideOut);
      }
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      if (isInterleave == 1) {
         AUDIOLIB_gainNChInterLeave_exec_cn<double>(pIn, pGain, pOut, widthIn, heightIn, strideIn, strideOut);
      }
      else {
         AUDIOLIB_gainNCh_exec_cn<double>(pIn, pGain, pOut, widthIn, heightIn, strideIn, strideOut);
      }
   }
}
