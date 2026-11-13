// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_matTrans_cn.h"
#include <dsplib.h>

/* Write each column of the input to a row of the output */
template <typename dataType>
static void
DSPLIB_matTrans_exec_cn(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn, size_t strideOut)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (size_t i = 0; i < widthIn; i++) {
      for (size_t j = 0; j < heightIn; j++) {
         pOutLocal[(i * strideOut) + j] = pInLocal[i + (strideIn * j)];
      }
   }
}

void DSPLIB_matTrans_cn(void    *pIn,
                        void    *pOut,
                        uint32_t dataType,
                        size_t   widthIn,
                        size_t   heightIn,
                        size_t   strideIn,
                        size_t   strideOut)
{
   if (dataType == DSPLIB_FLOAT32) {
      DSPLIB_matTrans_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (dataType == DSPLIB_FLOAT64) {
      DSPLIB_matTrans_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
}
