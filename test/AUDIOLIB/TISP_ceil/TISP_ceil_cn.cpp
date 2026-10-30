// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_ceil_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_ceil_exec_cn(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn, size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         pOutLocal[i * strideOut + j] = (dataType) ceil((double) pInLocal[i * strideIn + j]);
      }
   }
   return;
}

template void
AUDIOLIB_ceil_exec_cn<float>(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn, size_t strideOut);
template void AUDIOLIB_ceil_exec_cn<double>(void  *pIn,
                                            void  *pOut,
                                            size_t widthIn,
                                            size_t heightIn,
                                            size_t strideIn,
                                            size_t strideOut);
void          AUDIOLIB_ceil_cn(void    *pIn,
                               void    *pOut,
                               uint32_t data_type,
                               size_t   widthIn,
                               size_t   heightIn,
                               size_t   strideIn,
                               size_t   strideOut)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_ceil_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_ceil_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
}
