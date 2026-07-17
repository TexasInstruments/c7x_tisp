// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_tableInterpolate_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_tableInterpolate_exec_cn(void  *pIn,
                                       void  *pTableinterpolate,
                                       void  *pOut,
                                       size_t srcSamples,
                                       size_t tableSamples,
                                       float  minVal,
                                       float  divisor)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocalSrc   = (dataType *) pIn;
   dataType *restrict pInLocalTable = (dataType *) pTableinterpolate;
   dataType *restrict pOutLocalOut  = (dataType *) pOut;

   float fract = 0;

   for (uint32_t i = 0; i < srcSamples; i++) {
      dataType sampleSrc = pInLocalSrc[i];
      uint32_t index     = 0;

      sampleSrc -= minVal;
      sampleSrc *= divisor;

      if (sampleSrc <= 0) {
         pOutLocalOut[i] = pInLocalTable[0];
      }
      else if (sampleSrc >= (tableSamples - 1)) {

         pOutLocalOut[i] = pInLocalTable[tableSamples - 1];
      }
      else {
         index           = floorf(sampleSrc);
         fract           = sampleSrc - index;
         pOutLocalOut[i] = pInLocalTable[index] * (1 - fract) + pInLocalTable[index + 1] * (fract);
      }
   }
   return;
}

template void AUDIOLIB_tableInterpolate_exec_cn<float>(void  *pIn,
                                                       void  *pTableinterpolate,
                                                       void  *pOut,
                                                       size_t srcSamples,
                                                       size_t tableSamples,
                                                       float  minVal,
                                                       float  divisor);

void AUDIOLIB_tableInterpolate_cn(void    *pIn,
                                  void    *pTableinterpolate,
                                  void    *pOut,
                                  uint32_t data_type,
                                  size_t   srcSamples,
                                  size_t   tableSamples,
                                  float    minVal,
                                  float    divisor)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_tableInterpolate_exec_cn<float>(pIn, pTableinterpolate, pOut, srcSamples, tableSamples, minVal, divisor);
   }
}
