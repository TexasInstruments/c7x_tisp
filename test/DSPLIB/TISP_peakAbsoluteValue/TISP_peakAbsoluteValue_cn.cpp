// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_peakAbsoluteValue_cn.h"
#include <dsplib.h>
#include <limits>

template <typename dataType>
void DSPLIB_peakAbsoluteValue_exec_cn(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      dataType out = 0;
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType x = pInLocal[i * strideIn + j];
         /* Integers equal to the type minimum saturate to the type maximum, as the C7x VABS instructions do */
         dataType a = (x == std::numeric_limits<dataType>::lowest()) ? std::numeric_limits<dataType>::max()
                                                                      : ((x < 0) ? (dataType) -x : x);
         out        = (a > out) ? a : out;
      }
      /* One peak per row */
      pOutLocal[i] = out;
   }
   return;
}

void DSPLIB_peakAbsoluteValue_cn(void *pIn, void *pOut, uint32_t data_type, size_t widthIn, size_t heightIn, size_t strideIn)
{
   if (data_type == DSPLIB_INT8) {
      DSPLIB_peakAbsoluteValue_exec_cn<int8_t>(pIn, pOut, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_INT16) {
      DSPLIB_peakAbsoluteValue_exec_cn<int16_t>(pIn, pOut, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_INT32) {
      DSPLIB_peakAbsoluteValue_exec_cn<int32_t>(pIn, pOut, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_INT64) {
      DSPLIB_peakAbsoluteValue_exec_cn<int64_t>(pIn, pOut, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_peakAbsoluteValue_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_peakAbsoluteValue_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn);
   }
}
