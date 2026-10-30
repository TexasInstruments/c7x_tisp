// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_logicalNOT_cn.h"
#include <dsplib.h>

template <typename dataType>
void DSPLIB_logicalNOT_exec_cn(void  *pIn,
                               void  *pOut,
                               size_t widthIn,
                               size_t heightIn,
                               size_t strideIn,
                               size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType x                   = pInLocal[i * strideIn + j];
         pOutLocal[i * strideOut + j] = (x == 0) ? (dataType) 1 : (dataType) 0;
      }
   }
   return;
}

void DSPLIB_logicalNOT_cn(void    *pIn,
                          void    *pOut,
                          uint32_t data_type,
                          size_t   widthIn,
                          size_t   heightIn,
                          size_t   strideIn,
                          size_t   strideOut)
{
   if (data_type == DSPLIB_INT8) {
      DSPLIB_logicalNOT_exec_cn<int8_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT8) {
      DSPLIB_logicalNOT_exec_cn<uint8_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT16) {
      DSPLIB_logicalNOT_exec_cn<int16_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT16) {
      DSPLIB_logicalNOT_exec_cn<uint16_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT32) {
      DSPLIB_logicalNOT_exec_cn<int32_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_logicalNOT_exec_cn<uint32_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT64) {
      DSPLIB_logicalNOT_exec_cn<int64_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT64) {
      DSPLIB_logicalNOT_exec_cn<uint64_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_logicalNOT_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_logicalNOT_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
}
