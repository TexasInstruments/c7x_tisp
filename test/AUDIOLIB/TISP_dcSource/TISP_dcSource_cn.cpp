// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_dcSource_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_dcSource_exec_cn(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut)
{
   uint32_t i, j;
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn1;
   dataType *pOutLocal = (dataType *) pOut;

   dataType constant = *pInLocal;

   for (i = 0; i < heightIn; i++) {
      for (j = 0; j < widthIn; j++) {
         pOutLocal[i * strideOut + j] = constant;
      }
   }

   return;
}

template void
AUDIOLIB_dcSource_exec_cn<float>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<double>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<int8_t>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<uint8_t>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<int16_t>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<uint16_t>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<int32_t>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<uint32_t>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<uint64_t>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);
template void
AUDIOLIB_dcSource_exec_cn<int64_t>(void *pIn1, void *pOut, size_t widthIn, size_t heightIn, size_t strideOut);

void AUDIOLIB_dcSource_cn(void *pIn, void *pOut, uint32_t data_type, size_t widthIn, size_t heightIn, size_t strideOut)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_dcSource_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_dcSource_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_INT8) {
      AUDIOLIB_dcSource_exec_cn<int8_t>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_UINT8) {
      AUDIOLIB_dcSource_exec_cn<uint8_t>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_INT16) {
      AUDIOLIB_dcSource_exec_cn<int16_t>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_UINT16) {
      AUDIOLIB_dcSource_exec_cn<uint16_t>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_INT32) {
      AUDIOLIB_dcSource_exec_cn<int32_t>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_UINT32) {
      AUDIOLIB_dcSource_exec_cn<uint32_t>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_INT64) {
      AUDIOLIB_dcSource_exec_cn<int64_t>(pIn, pOut, widthIn, heightIn, strideOut);
   }
   else if (data_type == AUDIOLIB_UINT64) {
      AUDIOLIB_dcSource_exec_cn<uint64_t>(pIn, pOut, widthIn, heightIn, strideOut);
   }
}
