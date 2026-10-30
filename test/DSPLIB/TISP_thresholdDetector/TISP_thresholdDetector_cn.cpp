// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_thresholdDetector_cn.h"
#include <dsplib.h>

template <typename dataTypeIn, typename dataTypeOut>
void DSPLIB_thresholdDetector_exec_cn(void    *pIn,
                                      void    *pOut,
                                      uint32_t opType,
                                      double   constant,
                                      size_t   widthIn,
                                      size_t   heightIn,
                                      size_t   strideIn,
                                      size_t   strideOut)
{
   /* Typecast void pointers to respective data type */
   dataTypeIn  *pInLocal  = (dataTypeIn *) pIn;
   dataTypeOut *pOutLocal = (dataTypeOut *) pOut;
   dataTypeIn   value     = (dataTypeIn) constant;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         dataTypeIn x      = pInLocal[i * strideIn + j];
         bool       result = false;
         switch (opType) {
         case DSPLIB_THRESHOLDDETECTOR_EQ:
            result = (x == value);
            break;
         case DSPLIB_THRESHOLDDETECTOR_NE:
            result = (x != value);
            break;
         case DSPLIB_THRESHOLDDETECTOR_LT:
            result = (x < value);
            break;
         case DSPLIB_THRESHOLDDETECTOR_LE:
            result = (x <= value);
            break;
         case DSPLIB_THRESHOLDDETECTOR_GT:
            result = (x > value);
            break;
         default:
            result = (x >= value);
            break;
         }
         pOutLocal[i * strideOut + j] = (dataTypeOut) result;
      }
   }
   return;
}

void DSPLIB_thresholdDetector_cn(void    *pIn,
                                 void    *pOut,
                                 uint32_t data_type,
                                 uint32_t opType,
                                 double   constant,
                                 size_t   widthIn,
                                 size_t   heightIn,
                                 size_t   strideIn,
                                 size_t   strideOut)
{
   if (data_type == DSPLIB_INT8) {
      DSPLIB_thresholdDetector_exec_cn<int8_t, uint8_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                        strideOut);
   }
   else if (data_type == DSPLIB_UINT8) {
      DSPLIB_thresholdDetector_exec_cn<uint8_t, uint8_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                         strideOut);
   }
   else if (data_type == DSPLIB_INT16) {
      DSPLIB_thresholdDetector_exec_cn<int16_t, uint16_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                          strideOut);
   }
   else if (data_type == DSPLIB_UINT16) {
      DSPLIB_thresholdDetector_exec_cn<uint16_t, uint16_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                           strideOut);
   }
   else if (data_type == DSPLIB_INT32) {
      DSPLIB_thresholdDetector_exec_cn<int32_t, uint32_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                          strideOut);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_thresholdDetector_exec_cn<uint32_t, uint32_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                           strideOut);
   }
   else if (data_type == DSPLIB_INT64) {
      DSPLIB_thresholdDetector_exec_cn<int64_t, uint64_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                          strideOut);
   }
   else if (data_type == DSPLIB_UINT64) {
      DSPLIB_thresholdDetector_exec_cn<uint64_t, uint64_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                           strideOut);
   }
   else if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_thresholdDetector_exec_cn<float, uint32_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                        strideOut);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_thresholdDetector_exec_cn<double, uint64_t>(pIn, pOut, opType, constant, widthIn, heightIn, strideIn,
                                                         strideOut);
   }
}
