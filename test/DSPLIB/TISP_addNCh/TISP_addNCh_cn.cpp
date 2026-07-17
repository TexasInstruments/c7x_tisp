// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_addNCh_cn.h"
#include <dsplib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void DSPLIB_addNCh_exec_cn(void  *pIn,
                           void  *pOut,
                           size_t widthIn,
                           size_t heightIn,
                           size_t inputs,
                           size_t strideInElementsY,
                           size_t strideInElementsZ,
                           size_t strideOutElements)
{

   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   dataType temp = 0;
   for (size_t row = 0; row < heightIn; row++) {
      for (size_t col = 0; col < widthIn; col++) {
         temp = 0;
         for (size_t layer = 0; layer < inputs; layer++) {
            temp += pInLocal[(row * strideInElementsY) + col + (strideInElementsZ * layer)];
         }
         pOutLocal[(row * strideOutElements) + col] = temp;
      }
   }

   return;
}

template void DSPLIB_addNCh_exec_cn<float>(void  *pIn,
                                           void  *pOut,
                                           size_t widthIn,
                                           size_t heightIn,
                                           size_t inputs,
                                           size_t strideInElementsY,
                                           size_t strideInElementsZ,
                                           size_t strideOutElements);

template void DSPLIB_addNCh_exec_cn<double>(void  *pIn,
                                            void  *pOut,
                                            size_t widthIn,
                                            size_t heightIn,
                                            size_t inputs,
                                            size_t strideInElementsY,
                                            size_t strideInElementsZ,
                                            size_t strideOutElements);

void DSPLIB_addNCh_cn(void    *pIn,
                      void    *pOut,
                      uint32_t data_type,
                      size_t   widthIn,
                      size_t   heightIn,
                      size_t   inputs,
                      size_t   strideInElementsY,
                      size_t   strideInElementsZ,
                      size_t   strideOutElements)
{
   if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_addNCh_exec_cn<float>(pIn, pOut, widthIn, heightIn, inputs, strideInElementsY, strideInElementsZ,
                                   strideOutElements);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_addNCh_exec_cn<double>(pIn, pOut, widthIn, heightIn, inputs, strideInElementsY, strideInElementsZ,
                                    strideOutElements);
   }
}
