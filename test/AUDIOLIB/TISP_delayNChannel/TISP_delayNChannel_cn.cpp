// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_delayNChannel_cn.h"
#include <audiolib.h>

#define MAX_NUM_CHANNELS 64 // Maximum number of channels supported

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

extern uint32_t writeIdxCn[MAX_NUM_CHANNELS];
extern uint32_t readIdxCn[MAX_NUM_CHANNELS];

template <typename dataType>
void AUDIOLIB_delayNChannel_interleave_exec_cn(void     *pIn,
                                               void     *pDelay,
                                               void     *pScratch,
                                               void     *pOut,
                                               size_t    widthIn,
                                               size_t    heightIn,
                                               size_t    mode,
                                               uint32_t *delaySize,
                                               size_t    delayBuffSize,
                                               size_t    strideInElements,
                                               size_t    strideOutElements,
                                               size_t    strideDelayElements)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocal      = (dataType *) pIn;
   dataType *restrict pOutLocal     = (dataType *) pOut;
   dataType *restrict pDelayLocal   = (dataType *) pDelay;
   dataType *restrict pScratchLocal = (dataType *) pScratch;

   uint32_t i, j;
   uint32_t chDelaySize;

   // Linear Delay Implementation
   if (mode == 0) {
      for (i = 0; i < heightIn; i++) {
         chDelaySize = delaySize[i];
         if (chDelaySize <= widthIn) {
            for (j = 0; j < chDelaySize; j++) {
               // Copy data from pDelay buffer to pOut buffer
               pScratchLocal[i * widthIn + j] = pDelayLocal[i * strideDelayElements + j];
               // Copy rest of the data from pIn buffer to pDelay buffer
               pDelayLocal[i * strideDelayElements + j] = pInLocal[strideInElements * (widthIn - chDelaySize + j) + i];
            }

            // Copy data from pIn buffer to pOut buffer
            for (j = 0; j < (widthIn - chDelaySize); j++) {
               pScratchLocal[(i * widthIn) + chDelaySize + j] = pInLocal[j * strideInElements + i];
            }
         }
         else {
            // Copy data from pDelay buffer to pOut buffer
            for (j = 0; j < widthIn; j++) {
               pScratchLocal[i * widthIn + j] = pDelayLocal[i * strideDelayElements + j];
            }

            // Left Shift the remaining elements within pDelay buffer
            for (j = 0; j < (chDelaySize - widthIn); j++) {
               pDelayLocal[i * strideDelayElements + j] = pDelayLocal[i * strideDelayElements + widthIn + j];
            }

            // Copy data from pIn buffer to pDelay buffer
            for (j = 0; j < widthIn; j++) {
               pDelayLocal[i * strideDelayElements + (chDelaySize - widthIn) + j] = pInLocal[j * strideInElements + i];
            }
         }
      }
   }
   // Circular Delay Implementation
   else if (mode == 1) {
      uint32_t readIdx = 0, writeIdx = 0;
      uint32_t loopWriteCnt1, loopWriteCnt2;
      uint32_t loopReadCnt1, loopReadCnt2;

      for (i = 0; i < heightIn; i++) {

         readIdx  = readIdxCn[i];
         writeIdx = writeIdxCn[i];

         loopWriteCnt1 = (delayBuffSize - writeIdx) <= widthIn ? (delayBuffSize - writeIdx) : widthIn;
         loopWriteCnt2 = widthIn - loopWriteCnt1;

         loopReadCnt1 = (delayBuffSize - readIdx) <= widthIn ? (delayBuffSize - readIdx) : widthIn;
         loopReadCnt2 = widthIn - loopReadCnt1;

         // Write Input samples from pIn buffer to pDelay buffer
         for (j = 0; j < loopWriteCnt1; j++) {
            pDelayLocal[i * strideDelayElements + writeIdx + j] = pInLocal[j * strideInElements + i];
         }
         writeIdx = (writeIdx + loopWriteCnt1) % delayBuffSize;

         if (loopWriteCnt2 > 0) {
            for (j = 0; j < loopWriteCnt2; j++) {
               pDelayLocal[i * strideDelayElements + j] = pInLocal[strideInElements * (loopWriteCnt1 + j) + i];
            }
            writeIdx = loopWriteCnt2;
         }

         // Read samples from pDelay buffer and write to pOut (pScratch) buffer
         for (j = 0; j < loopReadCnt1; j++) {
            pScratchLocal[i * widthIn + j] = pDelayLocal[i * strideDelayElements + readIdx + j];
         }
         readIdx = (readIdx + loopReadCnt1) % delayBuffSize;

         if (loopReadCnt2 > 0) {
            for (j = 0; j < loopReadCnt2; j++) {
               pScratchLocal[i * widthIn + loopReadCnt1 + j] = pDelayLocal[i * strideDelayElements + j];
            }
            readIdx = loopReadCnt2;
         }

         readIdxCn[i]  = readIdx;
         writeIdxCn[i] = writeIdx;
      }
   }
   for (i = 0; i < widthIn; i++) {
      for (j = 0; j < heightIn; j++) {
         pOutLocal[i * strideOutElements + j] = pScratchLocal[i + widthIn * j];
      }
   }

   return;
}

template void AUDIOLIB_delayNChannel_interleave_exec_cn<float>(void     *pIn,
                                                               void     *pDelay,
                                                               void     *pScratch,
                                                               void     *pOut,
                                                               size_t    widthIn,
                                                               size_t    heightIn,
                                                               size_t    mode,
                                                               uint32_t *delaySize,
                                                               size_t    delayBuffSize,
                                                               size_t    strideInElements,
                                                               size_t    strideOutElements,
                                                               size_t    strideDelayElements);
template void AUDIOLIB_delayNChannel_interleave_exec_cn<double>(void     *pIn,
                                                                void     *pDelay,
                                                                void     *pScratch,
                                                                void     *pOut,
                                                                size_t    widthIn,
                                                                size_t    heightIn,
                                                                size_t    mode,
                                                                uint32_t *delay,
                                                                size_t    delayBuffSize,
                                                                size_t    strideInElements,
                                                                size_t    strideOutElements,
                                                                size_t    strideDelayElements);

template <typename dataType>
void AUDIOLIB_delayNChannel_exec_cn(void     *pIn,
                                    void     *pDelay,
                                    void     *pScratch,
                                    void     *pOut,
                                    size_t    widthIn,
                                    size_t    heightIn,
                                    size_t    mode,
                                    uint32_t *delaySize,
                                    size_t    delayBuffSize,
                                    size_t    strideInElements,
                                    size_t    strideOutElements,
                                    size_t    strideDelayElements)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocal      = (dataType *) pIn;
   dataType *restrict pOutLocal     = (dataType *) pOut;
   dataType *restrict pDelayLocal   = (dataType *) pDelay;
   dataType *restrict pScratchLocal = (dataType *) pScratch;

   uint32_t i, j;
   uint32_t chDelaySize;

   // Linear Delay Implementation
   if (mode == 0) {
      for (i = 0; i < heightIn; i++) {
         chDelaySize = delaySize[i];
         if (chDelaySize <= widthIn) {
            for (j = 0; j < chDelaySize; j++) {
               // Copy data from pDelay buffer to pOut buffer
               pOutLocal[i * strideOutElements + j] = pDelayLocal[i * strideDelayElements + j];
               // Copy rest of the data from pIn buffer to pDelay buffer
               pDelayLocal[i * strideDelayElements + j] = pInLocal[i * strideInElements + (widthIn - chDelaySize) + j];
            }

            // Copy data from pIn buffer to pOut buffer
            for (j = 0; j < (widthIn - chDelaySize); j++) {
               pOutLocal[(i * strideOutElements) + chDelaySize + j] = pInLocal[i * strideInElements + j];
            }
         }
         else {
            // Copy data from pDelay buffer to pOut buffer
            for (j = 0; j < widthIn; j++) {
               pOutLocal[i * strideOutElements + j] = pDelayLocal[i * strideDelayElements + j];
            }

            // Left Shift the remaining elements within pDelay buffer
            for (j = 0; j < (chDelaySize - widthIn); j++) {
               pDelayLocal[i * strideDelayElements + j] = pDelayLocal[i * strideDelayElements + widthIn + j];
            }

            // Copy data from pIn buffer to pDelay buffer
            for (j = 0; j < widthIn; j++) {
               pDelayLocal[i * strideDelayElements + (chDelaySize - widthIn) + j] = pInLocal[i * strideInElements + j];
            }
         }
      }
   }
   // Circular Delay Implementation
   else if (mode == 1) {
      uint32_t readIdx = 0, writeIdx = 0;
      uint32_t loopWriteCnt1, loopWriteCnt2;
      uint32_t loopReadCnt1, loopReadCnt2;

      for (i = 0; i < heightIn; i++) {

         readIdx  = readIdxCn[i];
         writeIdx = writeIdxCn[i];

         loopWriteCnt1 = (delayBuffSize - writeIdx) <= widthIn ? (delayBuffSize - writeIdx) : widthIn;
         loopWriteCnt2 = widthIn - loopWriteCnt1;

         loopReadCnt1 = (delayBuffSize - readIdx) <= widthIn ? (delayBuffSize - readIdx) : widthIn;
         loopReadCnt2 = widthIn - loopReadCnt1;

         // Write Input samples from pIn buffer to pDelay buffer
         for (j = 0; j < loopWriteCnt1; j++) {
            pDelayLocal[i * strideDelayElements + writeIdx + j] = pInLocal[i * strideInElements + j];
         }
         writeIdx = (writeIdx + loopWriteCnt1) % delayBuffSize;

         if (loopWriteCnt2 > 0) {
            for (j = 0; j < loopWriteCnt2; j++) {
               pDelayLocal[i * strideDelayElements + j] = pInLocal[i * strideInElements + loopWriteCnt1 + j];
            }
            writeIdx = loopWriteCnt2;
         }

         // Read samples from pDelay buffer and write to pOut (pScratch) buffer
         for (j = 0; j < loopReadCnt1; j++) {
            pOutLocal[i * strideOutElements + j] = pDelayLocal[i * strideDelayElements + readIdx + j];
         }
         readIdx = (readIdx + loopReadCnt1) % delayBuffSize;

         if (loopReadCnt2 > 0) {
            for (j = 0; j < loopReadCnt2; j++) {
               pOutLocal[i * strideOutElements + loopReadCnt1 + j] = pDelayLocal[i * strideDelayElements + j];
            }
            readIdx = loopReadCnt2;
         }

         readIdxCn[i]  = readIdx;
         writeIdxCn[i] = writeIdx;
      }
   }

   return;
}

template void AUDIOLIB_delayNChannel_exec_cn<float>(void     *pIn,
                                                    void     *pDelay,
                                                    void     *pScratch,
                                                    void     *pOut,
                                                    size_t    widthIn,
                                                    size_t    heightIn,
                                                    size_t    mode,
                                                    uint32_t *delaySize,
                                                    size_t    delayBuffSize,
                                                    size_t    strideInElements,
                                                    size_t    strideOutElements,
                                                    size_t    strideDelayElements);
template void AUDIOLIB_delayNChannel_exec_cn<double>(void     *pIn,
                                                     void     *pDelay,
                                                     void     *pScratch,
                                                     void     *pOut,
                                                     size_t    widthIn,
                                                     size_t    heightIn,
                                                     size_t    mode,
                                                     uint32_t *delaySize,
                                                     size_t    delayBuffSize,
                                                     size_t    strideInElements,
                                                     size_t    strideOutElements,
                                                     size_t    strideDelayElements);

void AUDIOLIB_delayNChannel_cn(void     *pIn,
                               void     *pDelay,
                               void     *pScratch,
                               void     *pOut,
                               uint32_t  data_type,
                               size_t    widthIn,
                               size_t    heightIn,
                               size_t    interleave,
                               size_t    mode,
                               uint32_t *delaySize,
                               size_t    delayBuffSize,
                               size_t    strideInElements,
                               size_t    strideOutElements,
                               size_t    strideDelayElements)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      if (interleave == 0) {
         AUDIOLIB_delayNChannel_exec_cn<float>(pIn, pDelay, pScratch, pOut, widthIn, heightIn, mode, delaySize,
                                               delayBuffSize, strideInElements, strideOutElements, strideDelayElements);
      }
      else if (interleave == 1) {
         AUDIOLIB_delayNChannel_interleave_exec_cn<float>(pIn, pDelay, pScratch, pOut, widthIn, heightIn, mode,
                                                          delaySize, delayBuffSize, strideInElements, strideOutElements,
                                                          strideDelayElements);
      }
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      if (interleave == 0) {
         AUDIOLIB_delayNChannel_exec_cn<double>(pIn, pDelay, pScratch, pOut, widthIn, heightIn, mode, delaySize,
                                                delayBuffSize, strideInElements, strideOutElements,
                                                strideDelayElements);
      }
      else if (interleave == 1) {
         AUDIOLIB_delayNChannel_interleave_exec_cn<double>(pIn, pDelay, pScratch, pOut, widthIn, heightIn, mode,
                                                           delaySize, delayBuffSize, strideInElements,
                                                           strideOutElements, strideDelayElements);
      }
   }
}
