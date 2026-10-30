// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_conditionalReplace_cn.h"
#include <dsplib.h>

void DSPLIB_conditionalReplace_cn(void  *pIn,
                                       void  *pOut,
                                       void  *pCompareType,
                                       void  *pConstValue,
                                       void  *pReplaceValue,
                                       size_t widthIn,
                                       size_t heightIn,
                                       size_t strideIn,
                                       size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   int32_t *pInLocal           = (int32_t *) pIn;
   int32_t *pOutLocal          = (int32_t *) pOut;
   int32_t *pCompareTypeLocal  = (int32_t *) pCompareType;
   int32_t *pConstValueLocal   = (int32_t *) pConstValue;
   int32_t *pReplaceValueLocal = (int32_t *) pReplaceValue;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         int32_t x   = pInLocal[i * strideIn + j];
         int32_t c   = pConstValueLocal[i];
         bool    hit = false;
         switch (pCompareTypeLocal[i]) {
         case DSPLIB_CONDITIONALREPLACE_EQ:
            hit = (x == c);
            break;
         case DSPLIB_CONDITIONALREPLACE_NE:
            hit = (x != c);
            break;
         case DSPLIB_CONDITIONALREPLACE_LT:
            hit = (x < c);
            break;
         case DSPLIB_CONDITIONALREPLACE_LE:
            hit = (x <= c);
            break;
         case DSPLIB_CONDITIONALREPLACE_GT:
            hit = (x > c);
            break;
         default:
            hit = (x >= c);
            break;
         }
         pOutLocal[i * strideOut + j] = hit ? pReplaceValueLocal[i] : x;
      }
   }
   return;
}
