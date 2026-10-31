// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_nonZeroCheck_cn.h"
#include <dsplib.h>

template <typename dataType>
void DSPLIB_nonZeroCheck_exec_cn(void    *pIn,
                                 void    *pOut,
                                 uint32_t opType,
                                 uint32_t axisType,
                                 size_t   widthIn,
                                 size_t   heightIn,
                                 size_t   strideIn)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   /* ROW axis: one result per column (reduce over the rows), COLUMN axis: one result per row (reduce over the
    * columns). ALL starts from 1 and clears it on a zero, ANY starts from 0 and sets it on a non-zero. */
   size_t outLen = (axisType == DSPLIB_NONZEROCHECK_ROW) ? widthIn : heightIn;
   for (size_t k = 0; k < outLen; k++) {
      dataType result = (opType == DSPLIB_NONZEROCHECK_ALL) ? (dataType) 1 : (dataType) 0;
      size_t   count  = (axisType == DSPLIB_NONZEROCHECK_ROW) ? heightIn : widthIn;
      for (size_t m = 0; m < count; m++) {
         dataType x = (axisType == DSPLIB_NONZEROCHECK_ROW) ? pInLocal[m * strideIn + k] : pInLocal[k * strideIn + m];
         if (opType == DSPLIB_NONZEROCHECK_ALL) {
            result = (x == 0) ? (dataType) 0 : result;
         }
         else {
            result = (x != 0) ? (dataType) 1 : result;
         }
      }
      pOutLocal[k] = result;
   }
   return;
}

void DSPLIB_nonZeroCheck_cn(void    *pIn,
                            void    *pOut,
                            uint32_t data_type,
                            uint32_t opType,
                            uint32_t axisType,
                            size_t   widthIn,
                            size_t   heightIn,
                            size_t   strideIn)
{
   if (data_type == DSPLIB_INT8) {
      DSPLIB_nonZeroCheck_exec_cn<int8_t>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_UINT8) {
      DSPLIB_nonZeroCheck_exec_cn<uint8_t>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_INT16) {
      DSPLIB_nonZeroCheck_exec_cn<int16_t>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_UINT16) {
      DSPLIB_nonZeroCheck_exec_cn<uint16_t>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_INT32) {
      DSPLIB_nonZeroCheck_exec_cn<int32_t>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_nonZeroCheck_exec_cn<uint32_t>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_INT64) {
      DSPLIB_nonZeroCheck_exec_cn<int64_t>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_UINT64) {
      DSPLIB_nonZeroCheck_exec_cn<uint64_t>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_nonZeroCheck_exec_cn<float>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_nonZeroCheck_exec_cn<double>(pIn, pOut, opType, axisType, widthIn, heightIn, strideIn);
   }
}
