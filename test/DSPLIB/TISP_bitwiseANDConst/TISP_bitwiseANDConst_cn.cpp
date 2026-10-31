// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_bitwiseANDConst_cn.h"
#include <dsplib.h>

template <typename dataType>
void DSPLIB_bitwiseANDConst_exec_cn(void    *pIn,
                                    void    *pOut,
                                    uint64_t constant,
                                    size_t   widthIn,
                                    size_t   heightIn,
                                    size_t   strideIn,
                                    size_t   strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         pOutLocal[i * strideOut + j] = pInLocal[i * strideIn + j] & (dataType) constant;
      }
   }
   return;
}

void DSPLIB_bitwiseANDConst_cn(void    *pIn,
                               void    *pOut,
                               uint32_t data_type,
                               uint64_t constant,
                               size_t   widthIn,
                               size_t   heightIn,
                               size_t   strideIn,
                               size_t   strideOut)
{
   if (data_type == DSPLIB_UINT8) {
      DSPLIB_bitwiseANDConst_exec_cn<uint8_t>(pIn, pOut, constant, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT16) {
      DSPLIB_bitwiseANDConst_exec_cn<uint16_t>(pIn, pOut, constant, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_bitwiseANDConst_exec_cn<uint32_t>(pIn, pOut, constant, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT64) {
      DSPLIB_bitwiseANDConst_exec_cn<uint64_t>(pIn, pOut, constant, widthIn, heightIn, strideIn, strideOut);
   }
}
