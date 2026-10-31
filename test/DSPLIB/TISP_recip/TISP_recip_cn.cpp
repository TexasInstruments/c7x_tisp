// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_recip_cn.h"
#include <dsplib.h>

template <typename dataType>
void DSPLIB_recip_exec_cn(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn, size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         pOutLocal[i * strideOut + j] = 1 / pInLocal[i * strideIn + j];
      }
   }
   return;
}

/* Q15 reciprocal: the exponent and the fraction of every input element, stored as HSTACK (storeStyle 1: exponents
 * of the row, then fractions of the row) or INTERLEAVE (storeStyle 2: exponent, fraction, exponent, fraction, ...) */
void DSPLIB_recip_exec_cn_q15(void    *pIn,
                              void    *pOut,
                              uint32_t storeStyle,
                              size_t   widthIn,
                              size_t   heightIn,
                              size_t   strideIn,
                              size_t   strideOut)
{
   int16_t *pInLocal  = (int16_t *) pIn;
   int16_t *pOutLocal = (int16_t *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         int32_t a        = pInLocal[i * strideIn + j];
         bool    negative = (a < 0);
         a                = negative ? -a : a;

         /* Number of redundant sign bits of the non-negative value (31 for zero) */
         int32_t normal = 0;
         while ((normal < 31) && ((a & 0x40000000) == 0)) {
            a = (int32_t) ((uint32_t) a << 1U);
            normal++;
         }

         /* 15 conditional subtract steps of the division 0x80000000 / a */
         uint32_t b = 0x80000000U;
         for (int32_t step = 0; step < 15; step++) {
            b = (b >= (uint32_t) a) ? (((b - (uint32_t) a) << 1U) + 1U) : (b << 1U);
         }

         int32_t fraction = (int32_t) (b & 0x7FFFU);
         fraction         = negative ? -fraction : fraction;

         if (storeStyle == 1U) {
            pOutLocal[i * strideOut + j]           = (int16_t) (normal - 15);
            pOutLocal[i * strideOut + widthIn + j] = (int16_t) fraction;
         }
         else {
            pOutLocal[i * strideOut + 2 * j]     = (int16_t) (normal - 15);
            pOutLocal[i * strideOut + 2 * j + 1] = (int16_t) fraction;
         }
      }
   }
   return;
}

void DSPLIB_recip_cn(void    *pIn,
                     void    *pOut,
                     uint32_t data_type,
                     uint32_t storeStyle,
                     size_t   widthIn,
                     size_t   heightIn,
                     size_t   strideIn,
                     size_t   strideOut)
{
   if (data_type == DSPLIB_INT16) {
      DSPLIB_recip_exec_cn_q15(pIn, pOut, storeStyle, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_recip_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_recip_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
}
