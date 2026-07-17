// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_split_cn.h"
#include <audiolib.h>
#include <cstdint>

template <typename T>
void split_cn_impl(void           *pIn,
                   void          **pOut,
                   uint32_t        numOutputs,
                   uint32_t        inSamples,
                   const uint32_t *outChannels,
                   uint32_t        strideInElements,
                   const uint32_t *strideOutElements,
                   uint8_t         isInterleave)
{
   T       *pInLocal        = static_cast<T *>(pIn);
   uint32_t inChannelOffset = 0;

   for (uint32_t i = 0; i < numOutputs; i++) {
      T       *pOutLocal = static_cast<T *>(pOut[i]);
      int32_t  channels  = static_cast<int32_t>(outChannels[i]);
      uint32_t strideOut = strideOutElements[i];

      if (isInterleave) {
         // Interleaved: samples in dim_y, channels in dim_x
         for (uint32_t s = 0; s < inSamples; s++) {
            for (int32_t c = 0; c < channels; c++) {
               pOutLocal[s * strideOut + c] = pInLocal[s * strideInElements + inChannelOffset + c];
            }
         }
      }
      else {
         // Planar: samples in dim_x, channels in dim_y
         for (int32_t c = 0; c < channels; c++) {
            for (uint32_t s = 0; s < inSamples; s++) {
               pOutLocal[c * strideOut + s] = pInLocal[(inChannelOffset + c) * strideInElements + s];
            }
         }
      }
      inChannelOffset += channels;
   }
}

void AUDIOLIB_split_cn(void           *pIn,
                       void          **pOut,
                       uint32_t        dataType,
                       uint32_t        numOutputs,
                       uint32_t        inSamples,
                       const uint32_t *outChannels,
                       uint32_t        strideInElements,
                       const uint32_t *strideOutElements,
                       uint8_t         isInterleave)
{
   if (dataType == AUDIOLIB_FLOAT32) {
      split_cn_impl<float>(pIn, pOut, numOutputs, inSamples, outChannels, strideInElements, strideOutElements,
                           isInterleave);
   }
   else if (dataType == AUDIOLIB_FLOAT64) {
      split_cn_impl<double>(pIn, pOut, numOutputs, inSamples, outChannels, strideInElements, strideOutElements,
                            isInterleave);
   }
}
