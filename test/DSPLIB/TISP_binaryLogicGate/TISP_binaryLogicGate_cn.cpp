// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_binaryLogicGate_cn.h"
#include <dsplib.h>

template <typename dataType>
void DSPLIB_binaryLogicGate_exec_cn(void    *pIn0,
                                    void    *pIn1,
                                    void    *pOut,
                                    uint32_t opType,
                                    size_t   widthIn,
                                    size_t   heightIn,
                                    size_t   strideIn,
                                    size_t   strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pIn0Local = (dataType *) pIn0;
   dataType *pIn1Local = (dataType *) pIn1;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType x0     = pIn0Local[i * strideIn + j];
         dataType x1     = pIn1Local[i * strideIn + j];
         dataType result = 0;
         switch (opType) {
         case DSPLIB_BINARYLOGICGATE_AND:
            result = x0 & x1;
            break;
         case DSPLIB_BINARYLOGICGATE_OR:
            result = x0 | x1;
            break;
         default:
            result = x0 ^ x1;
            break;
         }
         pOutLocal[i * strideOut + j] = result;
      }
   }
   return;
}

void DSPLIB_binaryLogicGate_cn(void    *pIn0,
                               void    *pIn1,
                               void    *pOut,
                               uint32_t data_type,
                               uint32_t opType,
                               size_t   widthIn,
                               size_t   heightIn,
                               size_t   strideIn,
                               size_t   strideOut)
{
   if (data_type == DSPLIB_UINT8) {
      DSPLIB_binaryLogicGate_exec_cn<uint8_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT16) {
      DSPLIB_binaryLogicGate_exec_cn<uint16_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_binaryLogicGate_exec_cn<uint32_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == DSPLIB_UINT64) {
      DSPLIB_binaryLogicGate_exec_cn<uint64_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn, strideOut);
   }
}
