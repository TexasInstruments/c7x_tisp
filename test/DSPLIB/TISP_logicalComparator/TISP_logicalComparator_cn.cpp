// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_logicalComparator_cn.h"
#include <dsplib.h>

template <typename dataTypeIn, typename dataTypeOut>
void DSPLIB_logicalComparator_exec_cn(void    *pIn0,
                                      void    *pIn1,
                                      void    *pOut,
                                      uint32_t opType,
                                      size_t   widthIn,
                                      size_t   heightIn,
                                      size_t   strideIn,
                                      size_t   strideOut)
{
   /* Typecast void pointers to respective data type */
   dataTypeIn  *pIn0Local = (dataTypeIn *) pIn0;
   dataTypeIn  *pIn1Local = (dataTypeIn *) pIn1;
   dataTypeOut *pOutLocal = (dataTypeOut *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         dataTypeIn x      = pIn0Local[i * strideIn + j];
         dataTypeIn y      = pIn1Local[i * strideIn + j];
         bool       result = false;
         switch (opType) {
         case DSPLIB_LOGICALCOMPARATOR_EQ:
            result = (x == y);
            break;
         case DSPLIB_LOGICALCOMPARATOR_NE:
            result = (x != y);
            break;
         case DSPLIB_LOGICALCOMPARATOR_LT:
            result = (x < y);
            break;
         case DSPLIB_LOGICALCOMPARATOR_LE:
            result = (x <= y);
            break;
         case DSPLIB_LOGICALCOMPARATOR_GT:
            result = (x > y);
            break;
         default:
            result = (x >= y);
            break;
         }
         pOutLocal[i * strideOut + j] = (dataTypeOut) result;
      }
   }
   return;
}

void DSPLIB_logicalComparator_cn(void    *pIn0,
                                 void    *pIn1,
                                 void    *pOut,
                                 uint32_t data_type,
                                 uint32_t opType,
                                 size_t   widthIn,
                                 size_t   heightIn,
                                 size_t   strideIn,
                                 size_t   strideOut)
{
   if (data_type == DSPLIB_INT8) {
      DSPLIB_logicalComparator_exec_cn<int8_t, uint8_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                        strideOut);
   }
   else if (data_type == DSPLIB_UINT8) {
      DSPLIB_logicalComparator_exec_cn<uint8_t, uint8_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                         strideOut);
   }
   else if (data_type == DSPLIB_INT16) {
      DSPLIB_logicalComparator_exec_cn<int16_t, uint16_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                          strideOut);
   }
   else if (data_type == DSPLIB_UINT16) {
      DSPLIB_logicalComparator_exec_cn<uint16_t, uint16_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                           strideOut);
   }
   else if (data_type == DSPLIB_INT32) {
      DSPLIB_logicalComparator_exec_cn<int32_t, uint32_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                          strideOut);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_logicalComparator_exec_cn<uint32_t, uint32_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                           strideOut);
   }
   else if (data_type == DSPLIB_INT64) {
      DSPLIB_logicalComparator_exec_cn<int64_t, uint64_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                          strideOut);
   }
   else if (data_type == DSPLIB_UINT64) {
      DSPLIB_logicalComparator_exec_cn<uint64_t, uint64_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                           strideOut);
   }
   else if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_logicalComparator_exec_cn<float, uint32_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                        strideOut);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_logicalComparator_exec_cn<double, uint64_t>(pIn0, pIn1, pOut, opType, widthIn, heightIn, strideIn,
                                                         strideOut);
   }
}
