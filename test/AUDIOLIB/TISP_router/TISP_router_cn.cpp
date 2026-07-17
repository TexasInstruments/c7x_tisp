// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_router_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

void AUDIOLIB_router_cn_set(void     *pC,
                            size_t    heightIn,
                            size_t    heightOut,
                            size_t    strideIn,
                            size_t    isInterleave,
                            int32_t  *muteArray,
                            uint32_t *outputOffsetArray)
{
   int32_t *pOutChannelLocal = (int32_t *) pC;

   uint32_t runOffsetArray[heightOut], runLengthArray[heightOut], runMuteArray[heightOut];

   uint32_t i        = 0;
   uint32_t runCount = 0;

   uint32_t offsetFactor = (isInterleave == 1) ? 1 : strideIn;

   while (i < heightOut) {
      uint32_t ch_idx = pOutChannelLocal[i];

      if (ch_idx >= heightIn) {
         outputOffsetArray[i]     = 0;
         muteArray[i]             = 0;
         runMuteArray[runCount]   = 0;
         runOffsetArray[runCount] = 0;
         runLengthArray[runCount] = 1;
         runCount++;
         i++;
         continue;
      }

      // valid channel → start a run
      uint32_t start           = i;
      outputOffsetArray[start] = ch_idx * offsetFactor;
      muteArray[start]         = 1;

      uint32_t j = i + 1;
      while (j < heightOut) {
         ch_idx = pOutChannelLocal[j];
         if (ch_idx >= heightIn)
            break;
         if (pOutChannelLocal[j] != pOutChannelLocal[j - 1] + 1)
            break;

         outputOffsetArray[j] = ch_idx * offsetFactor;
         muteArray[j]         = 1;
         j++;
      }

      // save run of valid channels
      runOffsetArray[runCount] = outputOffsetArray[start];
      runLengthArray[runCount] = j - start;
      runMuteArray[runCount]   = 1;
      runCount++;

      i = j; // continue from next channel
   }
   return;
}

template <typename dataType>
void AUDIOLIB_router_exec_cn(void     *pIn,
                             void     *pOut,
                             void     *pOutScratch,
                             size_t    widthIn,
                             size_t    heightIn,
                             size_t    strideIn,
                             size_t    strideOut,
                             size_t    cheightIn,
                             int32_t  *muteArray,
                             uint32_t *outputOffsetArray)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < cheightIn; i++) {
      int32_t muteVal = muteArray[i];
      for (uint32_t j = 0; j < widthIn; j++) {
         pOutLocal[i * strideOut + j] = pInLocal[outputOffsetArray[i] + j] * muteVal;
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_routerInterLeave_exec_cn(void     *pIn,
                                       void     *pOut,
                                       void     *pOutScratch,
                                       size_t    widthIn,
                                       size_t    heightIn,
                                       size_t    strideIn,
                                       size_t    strideOut,
                                       size_t    cheightIn,
                                       int32_t  *muteArray,
                                       uint32_t *outputOffsetArray)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < cheightIn; ++i) {
      int32_t  muteVal = muteArray[i];
      uint32_t base    = outputOffsetArray[i]; // starting index of this column

      for (uint32_t j = 0; j < heightIn; ++j) {
         dataType inVal               = pInLocal[base + j * strideIn];
         pOutLocal[j * strideOut + i] = inVal * muteVal;
      }
   }
   return;
}
template void AUDIOLIB_router_exec_cn<float>(void     *pIn,
                                             void     *pOut,
                                             void     *pOutScratch,
                                             size_t    widthIn,
                                             size_t    heightIn,
                                             size_t    strideIn,
                                             size_t    strideOut,
                                             size_t    cheightIn,
                                             int32_t  *muteArray,
                                             uint32_t *outputOffsetArray);
template void AUDIOLIB_router_exec_cn<double>(void     *pIn,
                                              void     *pOut,
                                              void     *pOutScratch,
                                              size_t    widthIn,
                                              size_t    heightIn,
                                              size_t    strideIn,
                                              size_t    strideOut,
                                              size_t    cheightIn,
                                              int32_t  *muteArray,
                                              uint32_t *outputOffsetArray);
template void AUDIOLIB_routerInterLeave_exec_cn<float>(void     *pIn,
                                                       void     *pOut,
                                                       void     *pOutScratch,
                                                       size_t    widthIn,
                                                       size_t    heightIn,
                                                       size_t    strideIn,
                                                       size_t    strideOut,
                                                       size_t    cheightIn,
                                                       int32_t  *muteArray,
                                                       uint32_t *outputOffsetArray);
template void AUDIOLIB_routerInterLeave_exec_cn<double>(void     *pIn,
                                                        void     *pOut,
                                                        void     *pOutScratch,
                                                        size_t    widthIn,
                                                        size_t    heightIn,
                                                        size_t    strideIn,
                                                        size_t    strideOut,
                                                        size_t    cheightIn,
                                                        int32_t  *muteArray,
                                                        uint32_t *outputOffsetArray);
void          AUDIOLIB_router_cn(void     *pIn,
                                 void     *pOut,
                                 void     *pOutScratch,
                                 uint32_t  data_type,
                                 size_t    widthIn,
                                 size_t    heightIn,
                                 size_t    strideIn,
                                 size_t    strideOut,
                                 size_t    cheightIn,
                                 int32_t  *muteArray,
                                 uint32_t *outputOffsetArray,
                                 size_t    isInterleave)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      if (isInterleave == 1) {
         AUDIOLIB_routerInterLeave_exec_cn<float>(pIn, pOut, pOutScratch, widthIn, heightIn, strideIn, strideOut,
                                                  cheightIn, muteArray, outputOffsetArray);
      }
      else {
         AUDIOLIB_router_exec_cn<float>(pIn, pOut, pOutScratch, widthIn, heightIn, strideIn, strideOut, cheightIn,
                                        muteArray, outputOffsetArray);
      }
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      if (isInterleave == 1) {
         AUDIOLIB_routerInterLeave_exec_cn<double>(pIn, pOut, pOutScratch, widthIn, heightIn, strideIn, strideOut,
                                                   cheightIn, muteArray, outputOffsetArray);
      }
      else {
         AUDIOLIB_router_exec_cn<double>(pIn, pOut, pOutScratch, widthIn, heightIn, strideIn, strideOut, cheightIn,
                                         muteArray, outputOffsetArray);
      }
   }
}
