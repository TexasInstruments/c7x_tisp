// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_concat_cn.h"
#include <audiolib.h>
#include <cstdint>

template <typename T>
void concat_cn_impl(void          **pIn,
                    void           *pOut,
                    uint32_t        numInputs,
                    const uint32_t *inChannels,
                    uint32_t        inSamples,
                    const uint32_t *strideInElements,
                    uint32_t        strideOutElements,
                    uint8_t         isInterleave)
{
   T       *pOutLocal        = static_cast<T *>(pOut);
   uint32_t outChannelOffset = 0;

   for (uint32_t i = 0; i < numInputs; i++) {
      T      *pInLocal = static_cast<T *>(pIn[i]);
      uint32_t chans   = inChannels[i];
      uint32_t strideIn = strideInElements[i];

      if (isInterleave) {
         // Interleaved: samples in dim_y, channels in dim_x
         for (uint32_t s = 0; s < inSamples; s++) {
            for (uint32_t c = 0; c < chans; c++) {
               pOutLocal[s * strideOutElements + outChannelOffset + c] = pInLocal[s * strideIn + c];
            }
         }
      }
      else {
         // Planar: samples in dim_x, channels in dim_y
         for (uint32_t c = 0; c < chans; c++) {
            for (uint32_t s = 0; s < inSamples; s++) {
               pOutLocal[(outChannelOffset + c) * strideOutElements + s] = pInLocal[c * strideIn + s];
            }
         }
      }
      outChannelOffset += chans;
   }
}

void AUDIOLIB_concat_cn(void          **pIn,
                        void           *pOut,
                        uint32_t        dataType,
                        uint32_t        numInputs,
                        const uint32_t *inChannels,
                        uint32_t        inSamples,
                        const uint32_t *strideInElements,
                        uint32_t        strideOutElements,
                        uint8_t         isInterleave)
{
   if (dataType == AUDIOLIB_FLOAT32) {
      concat_cn_impl<float>(pIn, pOut, numInputs, inChannels, inSamples, strideInElements, strideOutElements,
                            isInterleave);
   }
   else if (dataType == AUDIOLIB_FLOAT64) {
      concat_cn_impl<double>(pIn, pOut, numInputs, inChannels, inSamples, strideInElements, strideOutElements,
                             isInterleave);
   }
}
