// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_multiplexer_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_multiplexer_exec_cn(void   *pIn,
                                  void   *pOut,
                                  size_t  widthIn,
                                  size_t  heightIn,
                                  size_t  inputs,
                                  size_t *pPrevIdx,
                                  size_t *pCurrentIdx,
                                  size_t  strideInElementsY,
                                  size_t  strideInElementsZ,
                                  size_t  strideOutElements)
{
   /* Typecast void pointers to respective data type */

   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (*pCurrentIdx >= inputs) {
      *pCurrentIdx = *pPrevIdx;
   }

   for (size_t row = 0; row < heightIn; row++) {
      for (size_t col = 0; col < widthIn; col++) {

         /* Compute input address for the selected input index */
         size_t inOffset  = (row * strideInElementsY) + col + (strideInElementsZ * *pCurrentIdx);
         size_t outOffset = (row * strideOutElements) + col;

         /* Copy value directly (no accumulation) */
         pOutLocal[outOffset] = pInLocal[inOffset];
      }
   }
   return;
}

template void AUDIOLIB_multiplexer_exec_cn<float>(void   *pIn,
                                                  void   *pOut,
                                                  size_t  widthIn,
                                                  size_t  heightIn,
                                                  size_t  inputs,
                                                  size_t *pPrevIdx,
                                                  size_t *pCurrentIdx,
                                                  size_t  strideInElementsY,
                                                  size_t  strideInElementsZ,
                                                  size_t  strideOutElements);

template void AUDIOLIB_multiplexer_exec_cn<double>(void   *pIn,
                                                   void   *pOut,
                                                   size_t  widthIn,
                                                   size_t  heightIn,
                                                   size_t  inputs,
                                                   size_t *pPrevIdx,
                                                   size_t *pCurrentIdx,
                                                   size_t  strideInElementsY,
                                                   size_t  strideInElementsZ,
                                                   size_t  strideOutElements);

void AUDIOLIB_multiplexer_cn(void    *pIn,
                             void    *pOut,
                             size_t   widthIn,
                             size_t   heightIn,
                             size_t   inputs,
                             size_t  *pPrevIdx,
                             size_t  *pCurrentIdx,
                             size_t   strideInElementsY,
                             size_t   strideInElementsZ,
                             size_t   strideOutElements,
                             uint32_t data_type)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_multiplexer_exec_cn<float>(pIn, pOut, widthIn, heightIn, inputs, pPrevIdx, pCurrentIdx,
                                          strideInElementsY, strideInElementsZ, strideOutElements);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_multiplexer_exec_cn<double>(pIn, pOut, widthIn, heightIn, inputs, pPrevIdx, pCurrentIdx,
                                           strideInElementsY, strideInElementsZ, strideOutElements);
   }
}
