// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_undB10_cn.h"
#include <audiolib.h>
#include <math.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_undB10_exec_cn(void  *pIn,
                             void  *pOut,
                             size_t widthIn,
                             size_t heightIn,
                             size_t strideInElements,
                             size_t strideOutElements)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t m = 0; m < heightIn; m++) {
      for (uint32_t n = 0; n < widthIn; n++) {

         pOutLocal[m * strideOutElements + n] = exp((2.302585092994046 * pInLocal[(m * strideInElements) + n]) / 10);

         if (pOutLocal[m * strideOutElements + n] > 3.4028235e+38) {
            pOutLocal[m * strideOutElements + n] = 3.4028235e+38;
         }
         if (pOutLocal[m * strideOutElements + n] < 1.1754944e-38) {
            pOutLocal[m * strideOutElements + n] = 0.0;
         }
      }
   }

   return;
}

template void AUDIOLIB_undB10_exec_cn<float>(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t strideInElements,
                                             size_t strideOutElements);

void AUDIOLIB_undB10_cn(void  *pIn,
                        void  *pOut,
                        size_t widthIn,
                        size_t heightIn,
                        size_t strideInElements,
                        size_t strideOutElements)
{
   AUDIOLIB_undB10_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
}
