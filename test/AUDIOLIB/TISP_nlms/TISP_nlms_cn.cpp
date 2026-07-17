// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_nlms_cn.h"
#include <audiolib.h>
#include <math.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

extern int32_t readIdx;
extern int32_t writeIdx;

template <typename dataType>
void AUDIOLIB_nlms_exec_cn(void  *pIn,
                           void  *pInDesired,
                           void  *pStateBuffer,
                           void  *pScratchBuffer,
                           void  *pCoefficients,
                           void  *pOut,
                           size_t width,
                           size_t height,
                           size_t totalSamples,
                           size_t filterLength,
                           float  stepSize,
                           size_t circBuffMask,
                           float  regularization,
                           size_t strideInElements,
                           size_t strideOutElements,
                           size_t strideStateElements)
{
   /* Typecast void pointers to respective data type */

   dataType *restrict inputSignal   = static_cast<dataType *>(pIn);
   dataType *restrict desiredSignal = static_cast<dataType *>(pInDesired);
   dataType *restrict stateBuffer   = static_cast<dataType *>(pStateBuffer);
   dataType *restrict accumBuffer   = static_cast<dataType *>(pScratchBuffer);
   dataType *restrict coeffs        = static_cast<dataType *>(pCoefficients);
   dataType *restrict outputSignal  = static_cast<dataType *>(pOut);

   int32_t startReadIdx  = readIdx;
   int32_t startWriteIdx = writeIdx;

   int32_t nextReadIdx  = 0;
   int32_t nextWriteIdx = 0;

   for (uint32_t ch = 0; ch < height; ch++) {
      dataType *restrict chCoeffs  = coeffs + ch * strideInElements;
      dataType *restrict chState   = stateBuffer + ch * strideStateElements;
      dataType *restrict chAccum   = accumBuffer + ch * strideInElements;
      dataType *restrict chInput   = inputSignal + ch * strideInElements;
      dataType *restrict chDesired = desiredSignal + ch * strideOutElements;
      dataType *restrict chOutput  = outputSignal + ch * strideOutElements;

      int32_t readIdxCn  = startReadIdx;
      int32_t writeIdxCn = startWriteIdx;

      for (uint32_t i = 0; i < filterLength; i++) {
         chAccum[i] = static_cast<dataType>(0.0f);
      }

      for (int32_t sample = 0; sample < static_cast<int32_t>(totalSamples); sample++) {
         const dataType x_n = chInput[sample];

         chState[writeIdxCn] = x_n;

         dataType energy = static_cast<dataType>(0.0f);
         for (uint32_t i = 0; i < filterLength; i++) {
            int32_t  idx = (readIdxCn + i + 1) & circBuffMask;
            dataType val = chState[idx];
            energy += val * val;
         }

         dataType y_n = static_cast<dataType>(0.0f);

         for (uint32_t i = 0; i < filterLength; i++) {
            int32_t idx      = (readIdxCn + i + 1) & circBuffMask;
            int32_t coeffIdx = filterLength - 1 - i;
            y_n += chState[idx] * chCoeffs[coeffIdx];
         }

         chOutput[sample] = y_n;

         const dataType e_n = chDesired[sample] - y_n;

         dataType       energyClamped = (energy < static_cast<dataType>(1e-6f)) ? static_cast<dataType>(1e-6f) : energy;
         const dataType stepFactor =
             static_cast<dataType>(2.0f) * static_cast<dataType>(stepSize) * e_n / (energyClamped + regularization);

         for (uint32_t i = 0; i < filterLength; i++) {
            int32_t idx = (readIdxCn + i + 1) & circBuffMask;
            chAccum[filterLength - 1 - i] += stepFactor * chState[idx];
         }

         readIdxCn  = (readIdxCn + 1) & circBuffMask;
         writeIdxCn = (writeIdxCn + 1) & circBuffMask;
      }

      for (uint32_t i = 0; i < filterLength; i++) {
         chCoeffs[i] += chAccum[i];
      }

      nextReadIdx  = readIdxCn;
      nextWriteIdx = writeIdxCn;
   }

   readIdx  = nextReadIdx;
   writeIdx = nextWriteIdx;
}

template <typename dataType>
void AUDIOLIB_nlms_interleave_exec_cn(void  *pIn,
                                      void  *pInDesired,
                                      void  *pStateBuffer,
                                      void  *pScratchBuffer,
                                      void  *pCoefficients,
                                      void  *pOut,
                                      size_t width,
                                      size_t height,
                                      size_t totalSamples,
                                      size_t filterLength,
                                      float  stepSize,
                                      size_t circBuffMask,
                                      float  regularization,
                                      size_t strideInElements,
                                      size_t strideOutElements,
                                      size_t strideStateElements)
{
   /* Typecast void pointers to respective data type */

   dataType *restrict inputSignal   = static_cast<dataType *>(pIn);
   dataType *restrict desiredSignal = static_cast<dataType *>(pInDesired);
   dataType *restrict stateBuffer   = static_cast<dataType *>(pStateBuffer);
   dataType *restrict accumBuffer   = static_cast<dataType *>(pScratchBuffer);
   dataType *restrict coeffs        = static_cast<dataType *>(pCoefficients);
   dataType *restrict outputSignal  = static_cast<dataType *>(pOut);

   int32_t startReadIdx  = readIdx;
   int32_t startWriteIdx = writeIdx;

   int32_t nextReadIdx  = 0;
   int32_t nextWriteIdx = 0;

   for (uint32_t ch = 0; ch < width; ch++) {
      dataType *restrict chCoeffs = coeffs + ch * strideInElements;
      dataType *restrict chState  = stateBuffer + ch * strideStateElements;
      dataType *restrict chAccum  = accumBuffer + ch * strideInElements;

      int32_t readIdxCn  = startReadIdx;
      int32_t writeIdxCn = startWriteIdx;

      for (uint32_t i = 0; i < filterLength; i++) {
         chAccum[i] = static_cast<dataType>(0.0f);
      }

      for (int32_t sample = 0; sample < static_cast<int32_t>(totalSamples); sample++) {
         const int32_t idxIn  = sample * strideInElements + ch;
         const int32_t idxOut = sample * strideOutElements + ch;

         const dataType x_n = inputSignal[idxIn];

         chState[writeIdxCn] = x_n;

         dataType energy = static_cast<dataType>(0.0f);
         for (uint32_t i = 0; i < filterLength; i++) {
            // Accessing exact window elements
            int32_t  idx = (readIdxCn + i + 1) & circBuffMask;
            dataType val = chState[idx];
            energy += val * val;
         }

         dataType y_n = static_cast<dataType>(0.0f);
         for (uint32_t i = 0; i < filterLength; i++) {
            int32_t idx = (readIdxCn + i + 1) & circBuffMask;
            y_n += chState[idx] * chCoeffs[filterLength - 1 - i];
         }

         outputSignal[idxOut] = y_n;
         const dataType e_n   = desiredSignal[idxOut] - y_n;

         dataType energyClamped = (energy < static_cast<dataType>(1e-6f)) ? static_cast<dataType>(1e-6f) : energy;

         const dataType stepFactor =
             static_cast<dataType>(2.0f) * static_cast<dataType>(stepSize) * e_n / (energyClamped + regularization);

         for (uint32_t i = 0; i < filterLength; i++) {
            int32_t idx = (readIdxCn + i + 1) & circBuffMask;
            chAccum[filterLength - 1 - i] += stepFactor * chState[idx];
         }

         readIdxCn  = (readIdxCn + 1) & circBuffMask;
         writeIdxCn = (writeIdxCn + 1) & circBuffMask;
      }

      for (uint32_t i = 0; i < filterLength; i++) {
         chCoeffs[i] += chAccum[i];
      }

      nextReadIdx  = readIdxCn;
      nextWriteIdx = writeIdxCn;
   }

   readIdx  = nextReadIdx;
   writeIdx = nextWriteIdx;
}

template void AUDIOLIB_nlms_interleave_exec_cn<float>(void  *pIn,
                                                      void  *pInDesired,
                                                      void  *pStateBuffer,
                                                      void  *pScratchBuffer,
                                                      void  *pCoefficients,
                                                      void  *pOut,
                                                      size_t width,
                                                      size_t height,
                                                      size_t totalSamples,
                                                      size_t filterLength,
                                                      float  stepSize,
                                                      size_t circBuffMask,
                                                      float  regularization,
                                                      size_t strideInElements,
                                                      size_t strideOutElements,
                                                      size_t strideStateElements);

template void AUDIOLIB_nlms_exec_cn<float>(void  *pIn,
                                           void  *pInDesired,
                                           void  *pStateBuffer,
                                           void  *pScratchBuffer,
                                           void  *pCoefficients,
                                           void  *pOut,
                                           size_t width,
                                           size_t height,
                                           size_t totalSamples,
                                           size_t filterLength,
                                           float  stepSize,
                                           size_t circBuffMask,
                                           float  regularization,
                                           size_t strideInElements,
                                           size_t strideOutElements,
                                           size_t strideStateElements);

void AUDIOLIB_nlms_cn(void    *pIn,
                      void    *pInDesired,
                      void    *pStateBuffer,
                      void    *pScratchBuffer,
                      void    *pCoefficients,
                      void    *pOut,
                      size_t   width,
                      size_t   height,
                      size_t   totalSamples,
                      size_t   filterLength,
                      float    stepSize,
                      size_t   circBuffMask,
                      float    regularization,
                      size_t   strideInElements,
                      size_t   strideOutElements,
                      size_t   strideStateElements,
                      uint32_t data_type,
                      uint32_t isInterleave)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      if (isInterleave == 1) {
         AUDIOLIB_nlms_interleave_exec_cn<float>(pIn, pInDesired, pStateBuffer, pScratchBuffer, pCoefficients, pOut,
                                                 width, height, totalSamples, filterLength, stepSize, circBuffMask,
                                                 regularization, strideInElements, strideOutElements,
                                                 strideStateElements);
      }
      else {
         AUDIOLIB_nlms_exec_cn<float>(pIn, pInDesired, pStateBuffer, pScratchBuffer, pCoefficients, pOut, width, height,
                                      totalSamples, filterLength, stepSize, circBuffMask, regularization,
                                      strideInElements, strideOutElements, strideStateElements);
      }
   }
}
