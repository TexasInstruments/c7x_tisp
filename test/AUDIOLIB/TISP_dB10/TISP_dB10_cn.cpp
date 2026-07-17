// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_dB10_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

#define FLT_MAX_DB10 3.402823466E+38

#define DBL_MAX_DB10 1.7976931348623157e+308

#define CONST_MAX 3082.546875

#define CONST_FLOAT_MIN 42865786880.0

#define CONST_DOUBLE_MIN 1.843542739450977e+20

template <typename dataType>
void AUDIOLIB_dB10_exec_cn(void  *pIn,
                           void  *pOut,
                           size_t widthIn,
                           size_t heightIn,
                           size_t strideInElements,
                           size_t strideOutElements,
                           size_t data_type)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t m = 0; m < heightIn; m++) {
      for (uint32_t n = 0; n < widthIn; n++) {

         pOutLocal[m * strideOutElements + n] = 10 * log10(pInLocal[m * strideInElements + n]);

         if (data_type == AUDIOLIB_FLOAT32) {
            if (pInLocal[m * strideInElements + n] > FLT_MAX_DB10) {
               pOutLocal[m * strideOutElements + n] = CONST_MAX;
            }
            else if (pInLocal[m * strideInElements + n] <= 0) {
               pOutLocal[m * strideOutElements + n] = CONST_FLOAT_MIN;
            }
         }
         if (data_type == AUDIOLIB_FLOAT64) {
            if (pInLocal[m * strideInElements + n] > DBL_MAX_DB10) {
               pOutLocal[m * strideOutElements + n] = CONST_MAX;
            }
            else if (pInLocal[m * strideInElements + n] <= 0) {
               pOutLocal[m * strideOutElements + n] = CONST_DOUBLE_MIN;
            }
         }
      }
   }

   return;
}

template void AUDIOLIB_dB10_exec_cn<float>(void  *pIn,
                                           void  *pOut,
                                           size_t widthIn,
                                           size_t heightIn,
                                           size_t strideInElements,
                                           size_t strideOutElements,
                                           size_t data_type);
template void AUDIOLIB_dB10_exec_cn<double>(void  *pIn,
                                            void  *pOut,
                                            size_t widthIn,
                                            size_t heightIn,
                                            size_t strideInElements,
                                            size_t strideOutElements,
                                            size_t data_type);

void AUDIOLIB_dB10_cn(void    *pIn,
                      void    *pOut,
                      uint32_t data_type,
                      size_t   widthIn,
                      size_t   heightIn,
                      size_t   strideInElements,
                      size_t   strideOutElements)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_dB10_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements, data_type);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_dB10_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements, data_type);
   }
}
