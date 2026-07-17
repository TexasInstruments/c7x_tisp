// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_gain_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_gain_exec_cn(void *pIn, void *pGain, void *pOut, size_t widthIn, size_t heightIn, size_t strideElements)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal   = (dataType *) pIn;
   dataType *pGainLocal = (dataType *) pGain;
   dataType *pOutLocal  = (dataType *) pOut;

   dataType gain = *pGainLocal;

   for (uint32_t m = 0; m < heightIn; m++) {
      for (uint32_t n = 0; n < widthIn; n++) {
         pOutLocal[m * strideElements + n] = pInLocal[m * strideElements + n] * gain;
      }
   }

   return;
}

template void AUDIOLIB_gain_exec_cn<float>(void  *pIn,
                                           void  *pGain,
                                           void  *pOut,
                                           size_t widthIn,
                                           size_t heightIn,
                                           size_t strideElements);
template void AUDIOLIB_gain_exec_cn<double>(void  *pIn,
                                            void  *pGain,
                                            void  *pOut,
                                            size_t widthIn,
                                            size_t heightIn,
                                            size_t strideElements);

void AUDIOLIB_gain_cn(void    *pIn,
                      void    *pGain,
                      void    *pOut,
                      uint32_t data_type,
                      size_t   widthIn,
                      size_t   heightIn,
                      size_t   strideElements)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_gain_exec_cn<float>(pIn, pGain, pOut, widthIn, heightIn, strideElements);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_gain_exec_cn<double>(pIn, pGain, pOut, widthIn, heightIn, strideElements);
   }
}
