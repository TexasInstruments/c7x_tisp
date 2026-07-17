// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_limiter_dB20_cn.h"
#include <audiolib.h>

#define FLT_MAX_DB20 3.402823466E+38

#define DBL_MAX_DB20 1.7976931348623157e+308

#define CONST_MAX_DB20 6165.09375 // 20 times of 308.2546875;

#define CONST_FLOAT_MIN_DB20 85731573760.0 // 20 times of 42865786880.0

#define CONST_DOUBLE_MIN_DB20 3.6870854789e+20

template <typename dataType>
void TISP_limiter_dB20_exec_cn(void  *pIn,
                               void  *pOut,
                               size_t widthIn,
                               size_t heightIn,
                               size_t strideInElements,
                               size_t strideOutElements)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t m = 0; m < heightIn; m++) {
      for (uint32_t n = 0; n < widthIn; n++) {

         pOutLocal[m * strideOutElements + n] = 20 * log10(pInLocal[m * strideInElements + n]);

         if (std::is_same<float, dataType>::value) {
            if (pInLocal[m * strideInElements + n] > FLT_MAX_DB20) {
               pOutLocal[m * strideOutElements + n] = CONST_MAX_DB20;
            }
            else if (pInLocal[m * strideInElements + n] <= 0) {
               pOutLocal[m * strideOutElements + n] = CONST_FLOAT_MIN_DB20;
            }
         }
         if (std::is_same<double, dataType>::value) {
            if (pInLocal[m * strideInElements + n] > DBL_MAX_DB20) {
               pOutLocal[m * strideOutElements + n] = CONST_MAX_DB20;
            }
            else if (pInLocal[m * strideInElements + n] <= 0) {
               pOutLocal[m * strideOutElements + n] = CONST_DOUBLE_MIN_DB20;
            }
         }
      }
   }

   return;
}

template void TISP_limiter_dB20_exec_cn<float>(void  *pIn,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideInElements,
                                               size_t strideOutElements);
template void TISP_limiter_dB20_exec_cn<double>(void  *pIn,
                                                void  *pOut,
                                                size_t widthIn,
                                                size_t heightIn,
                                                size_t strideInElements,
                                                size_t strideOutElements);
