// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_realImagToComplex_cn.h"
#include <dsplib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void DSPLIB_realImagToComplex_exec_cn(void  *pIn0,
                                      void  *pIn1,
                                      void  *pOut,
                                      size_t widthIn,
                                      size_t heightIn,
                                      size_t strideIn,
                                      size_t strideOut)
{
   int32_t  dataSize = sizeof(dataType);
   uint32_t i, j;
   /* Typecast void pointers to respective data type */
   dataType *pIn0Local = (dataType *) pIn0;
   dataType *pIn1Local = (dataType *) pIn1;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t m = 0; m < heightIn; m++) {
      for (uint32_t n = 0; n < widthIn; n++) {
         uint32_t inIndex  = m * strideIn + n;
         uint32_t outIndex = m * strideOut + n * 2;

         pOutLocal[outIndex]     = pIn0Local[inIndex];
         pOutLocal[outIndex + 1] = pIn1Local[inIndex];
      }
   }

   return;
}

template void DSPLIB_realImagToComplex_exec_cn<float>(void  *pIn0,
                                                      void  *pIn1,
                                                      void  *pOut,
                                                      size_t widthIn,
                                                      size_t heightIn,
                                                      size_t strideIn,
                                                      size_t strideOut);
template void DSPLIB_realImagToComplex_exec_cn<double>(void  *pIn0,
                                                       void  *pIn1,
                                                       void  *pOut,
                                                       size_t widthIn,
                                                       size_t heightIn,
                                                       size_t strideIn,
                                                       size_t strideOut);
template void DSPLIB_realImagToComplex_exec_cn<int16_t>(void  *pIn0,
                                                        void  *pIn1,
                                                        void  *pOut,
                                                        size_t widthIn,
                                                        size_t heightIn,
                                                        size_t strideIn,
                                                        size_t strideOut);
template void DSPLIB_realImagToComplex_exec_cn<uint16_t>(void  *pIn0,
                                                         void  *pIn1,
                                                         void  *pOut,
                                                         size_t widthIn,
                                                         size_t heightIn,
                                                         size_t strideIn,
                                                         size_t strideOut);
template void DSPLIB_realImagToComplex_exec_cn<int32_t>(void  *pIn0,
                                                        void  *pIn1,
                                                        void  *pOut,
                                                        size_t widthIn,
                                                        size_t heightIn,
                                                        size_t strideIn,
                                                        size_t strideOut);
template void DSPLIB_realImagToComplex_exec_cn<uint32_t>(void  *pIn0,
                                                         void  *pIn1,
                                                         void  *pOut,
                                                         size_t widthIn,
                                                         size_t heightIn,
                                                         size_t strideIn,
                                                         size_t strideOut);

void DSPLIB_realImagToComplex_cn(void    *pIn0,
                                 void    *pIn1,
                                 void    *pOut,
                                 uint32_t data_type,
                                 size_t   widthIn,
                                 size_t   heightIn,
                                 size_t   strideIn,
                                 size_t   strideOut)
{
   if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_realImagToComplex_exec_cn<float>(pIn0, pIn1, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_realImagToComplex_exec_cn<double>(pIn0, pIn1, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT16) {
      DSPLIB_realImagToComplex_exec_cn<int16_t>(pIn0, pIn1, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT16) {
      DSPLIB_realImagToComplex_exec_cn<uint16_t>(pIn0, pIn1, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_INT32) {
      DSPLIB_realImagToComplex_exec_cn<int32_t>(pIn0, pIn1, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_realImagToComplex_exec_cn<uint32_t>(pIn0, pIn1, pOut, widthIn, heightIn, strideIn, strideOut);
   }
}
