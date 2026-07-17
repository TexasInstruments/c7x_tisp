// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_softClip_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_softClip_exec_cn(void  *pIn,
                               void  *pOut,
                               size_t widthIn,
                               size_t heightIn,
                               size_t strideInElements,
                               size_t strideOutElements,
                               float  threshold,
                               float  endKnee)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   float inp    = 0.0f;
   float absInp = 0.0f;
   float out    = 0.0f;

   for (uint32_t m = 0; m < heightIn; m++) {
      for (uint32_t n = 0; n < widthIn; n++) {
         inp    = pInLocal[m * strideInElements + n];
         absInp = fabsf(inp);

         if (absInp < threshold) {
            pOutLocal[m * strideOutElements + n] = pInLocal[m * strideInElements + n];
         }

         else {
            out = (absInp - threshold) / (absInp - 2.0f * threshold + endKnee);
            out = (endKnee - threshold) * out + threshold;

            if (inp < 0.0f) {
               pOutLocal[m * strideOutElements + n] = -out;
            }

            else {
               pOutLocal[m * strideOutElements + n] = out;
            }
         }
      }
   }

   return;
}

template void AUDIOLIB_softClip_exec_cn<float>(void  *pIn,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideInElements,
                                               size_t strideOutElements,
                                               float  threshold,
                                               float  endKnee);

void AUDIOLIB_softClip_cn(void  *pIn,
                          void  *pOut,
                          size_t widthIn,
                          size_t heightIn,
                          size_t strideInElements,
                          size_t strideoutElements,
                          float  threshold,
                          float  endKnee)
{
   AUDIOLIB_softClip_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideoutElements, threshold,
                                    endKnee);
}
