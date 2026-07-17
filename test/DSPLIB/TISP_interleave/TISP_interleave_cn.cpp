// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_interleave_cn.h"
#include <dsplib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void DSPLIB_matTrans_exec_cn(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn, size_t strideOut)
{
   int32_t  dataSize = sizeof(dataType);
   uint32_t i, j;
   /* Typecast void pointers to respective data type */
   dataType *RESTRICT pInLocal  = (dataType *) pIn;
   dataType *RESTRICT pOutLocal = (dataType *) pOut;

   for (i = 0; i < widthIn; i++) {
      for (j = 0; j < heightIn; j++) {
         pOutLocal[(i * strideOut / dataSize) + j] = pInLocal[i + ((strideIn / dataSize) * j)];
      }
   }

   return;
}

template void DSPLIB_matTrans_exec_cn<float>(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t strideIn,
                                             size_t strideOut);
template void DSPLIB_matTrans_exec_cn<double>(void  *pIn,
                                              void  *pOut,
                                              size_t widthIn,
                                              size_t heightIn,
                                              size_t strideIn,
                                              size_t strideOut);
template void DSPLIB_matTrans_exec_cn<int8_t>(void  *pIn,
                                              void  *pOut,
                                              size_t widthIn,
                                              size_t heightIn,
                                              size_t strideIn,
                                              size_t strideOut);
template void DSPLIB_matTrans_exec_cn<uint8_t>(void  *pIn,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideIn,
                                               size_t strideOut);
template void DSPLIB_matTrans_exec_cn<int16_t>(void  *pIn,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideIn,
                                               size_t strideOut);
template void DSPLIB_matTrans_exec_cn<uint16_t>(void  *pIn,
                                                void  *pOut,
                                                size_t widthIn,
                                                size_t heightIn,
                                                size_t strideIn,
                                                size_t strideOut);
template void DSPLIB_matTrans_exec_cn<int32_t>(void  *pIn,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideIn,
                                               size_t strideOut);
template void DSPLIB_matTrans_exec_cn<uint32_t>(void  *pIn,
                                                void  *pOut,
                                                size_t widthIn,
                                                size_t heightIn,
                                                size_t strideIn,
                                                size_t strideOut);
template void DSPLIB_matTrans_exec_cn<uint64_t>(void  *pIn,
                                                void  *pOut,
                                                size_t widthIn,
                                                size_t heightIn,
                                                size_t strideIn,
                                                size_t strideOut);
template void DSPLIB_matTrans_exec_cn<int64_t>(void  *pIn,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideIn,
                                               size_t strideOut);

void DSPLIB_interleave_cn(void    *pIn,
                          void    *pOut,
                          uint32_t data_type,
                          size_t   widthIn,
                          size_t   heightIn,
                          size_t   strideIn,
                          size_t   strideOut)
{
   if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_matTrans_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_matTrans_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT8) {
      DSPLIB_matTrans_exec_cn<int8_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT8) {
      DSPLIB_matTrans_exec_cn<uint8_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT16) {
      DSPLIB_matTrans_exec_cn<int16_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT16) {
      DSPLIB_matTrans_exec_cn<uint16_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT32) {
      DSPLIB_matTrans_exec_cn<int32_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_matTrans_exec_cn<uint32_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT64) {
      DSPLIB_matTrans_exec_cn<int64_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT64) {
      DSPLIB_matTrans_exec_cn<uint64_t>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
}
