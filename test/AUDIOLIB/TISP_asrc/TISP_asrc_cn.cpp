// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_asrc_cn.h"
#include "AUDIOLIB_asrc/AUDIOLIB_asrc_priv.h"
#include "common/AUDIOLIB_bufParams.h"
#include <algorithm>
#include <audiolib.h>
#include <cstddef>
#include <dsplib.h>
#include <math.h>

#define AUDIOLIB_ASRC_MIN(x, y) (((x) < (y)) ? (x) : (y))

#define AUDIOLIB_ASRC_NUMBER_OF_FILTER_PHASES (32U)

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

#if __C7X_VEC_SIZE_BITS__ == 256
#define ALPHA_COEFF_INDEX 8
#else
#define ALPHA_COEFF_INDEX 16
#endif

bool AUDIOLIB_asrc_cn_set(uint8_t               mode,
                          double                asrcRatio,
                          void                 *pNonInterleavedData,
                          void                 *pIn,
                          uint32_t              datatype,
                          uint32_t              isInterleave,
                          size_t                maxSampleCountPerBlock,
                          uint32_t              heightIn,
                          TISP_asrcStateParams *asrcStateParams)
{
   uint32_t dataType;
   int32_t  i;
   bool     status = true;
   dataType        = datatype;

   if (mode == AUDIOLIB_ASRC_MODE_RESET && pIn == nullptr) {
      status = false;
   }
   else if (mode == AUDIOLIB_ASRC_MODE_RESET && isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED &&
            pNonInterleavedData == nullptr) {
      status = false;
   }
   else if ((mode == AUDIOLIB_ASRC_MODE_SET) &&
            ((asrcRatio <= 0.033f) || (asrcRatio > AUDIOLIB_ASRC_MAX_IO_SAMPLE_RATE_RATIO))) {
      status = false;
   }
   else {
      status = true;
   }
   if (mode == AUDIOLIB_ASRC_MODE_RESET) {
      asrcStateParams->fracOutputsRemaining = 0.0;
      asrcStateParams->remBufCount          = 0.0;
      if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
         // if (pNonInterleavedData != nullptr) {
         if (maxSampleCountPerBlock >= AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS) {
            int32_t offset = (maxSampleCountPerBlock * AUDIOLIB_ASRC_DOUBLE_BUFFERING_FACTOR) -
                             AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS;
            for (i = 0; i < (int32_t) heightIn; i++) {
               float *pNonInterleavedDataLocal = (float *) pNonInterleavedData + offset;
               memset((void *) pNonInterleavedDataLocal, 0,
                      AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS * AUDIOLIB_sizeof(dataType));
               offset += maxSampleCountPerBlock * AUDIOLIB_ASRC_DOUBLE_BUFFERING_FACTOR;
            }
         }
         else {
            int32_t offset = AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS;
            for (i = 0; i < (int32_t) heightIn; i++) {
               float *pNonInterleavedDataLocal = (float *) pNonInterleavedData + offset;
               memset((void *) pNonInterleavedDataLocal, 0,
                      AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS * AUDIOLIB_sizeof(dataType));
               offset += AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS * AUDIOLIB_ASRC_DOUBLE_BUFFERING_FACTOR;
            }
         }
      }
      else {

         if (maxSampleCountPerBlock >= AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS) {
            int32_t offset = (maxSampleCountPerBlock * AUDIOLIB_ASRC_QUADRUPLE_BUFFERING_FACTOR) -
                             AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS;
            for (i = 0; i < (int32_t) heightIn; i++) {
               float *pInLocal = (float *) pIn + offset;
               memset((void *) pInLocal, 0, AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS * AUDIOLIB_sizeof(dataType));
               offset += maxSampleCountPerBlock * AUDIOLIB_ASRC_QUADRUPLE_BUFFERING_FACTOR;
            }
         }
         else {
            int32_t offset = AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS;
            for (i = 0; i < (int32_t) heightIn; i++) {
               float *pInLocal = (float *) pIn + offset;
               memset((void *) pInLocal, 0, AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS * AUDIOLIB_sizeof(dataType));
               offset += AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS * AUDIOLIB_ASRC_DOUBLE_BUFFERING_FACTOR;
            }
         }
      }
   }
   else {
      asrcStateParams->outputsPerInputRatio = asrcRatio;
   }
   return status;
}

template <typename dataType>
void AUDIOLIB_asrc_exec_cn_non_interleaved(void                 *pIn,
                                           void                 *pNonInterleavedData,
                                           void                 *pFiltCoeffs,
                                           void                 *pFilterRembuf,
                                           void                 *pOut,
                                           size_t                widthOut,
                                           size_t                heightIn,
                                           TISP_asrcStateParams *asrcStateParams,
                                           uint32_t              inputSampleCnt,
                                           double                maxOutputsPerInputRatio,
                                           size_t                strideInElements,
                                           size_t                strideOutElements,
                                           size_t                isInterleaved,
                                           size_t                filterRembufWidthIn,
                                           uint32_t              inBufferTotalDimX,
                                           size_t                frameModuloFactor)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocal        = (dataType *) pIn;
   dataType *restrict pOutLocal       = (dataType *) pOut;
   dataType       *pFilterRembufLocal = (dataType *) pFilterRembuf;
   const dataType *pFiltCoeffsLocal   = (dataType *) pFiltCoeffs;
   const uint8_t   numChannels        = heightIn;
   dataType        hq;
   uint32_t       *cirBuffStartIndex = &asrcStateParams->cirBuffStartIdx;
   uint32_t        inputSampleCount  = inputSampleCnt;
   int32_t         preCompOutputCountInt;
   int32_t         j, k, outSampleIdx;
   size_t          i, n;

   /* output / input time step
    maxOutputsPerInputRatio will set the maximum rate ratio change acceptable*/
   /* Accumulator increment value. This variable stores the accumulator increment value, which is calculated as
    * 1/asrcRatio.
    */
   const double rho = 1 / AUDIOLIB_ASRC_MIN(asrcStateParams->outputsPerInputRatio, maxOutputsPerInputRatio);
   /** Accumulator value. This variable stores the accumulator value used in the ARC2 core processing.
    */
   const double tau = rho * (1 - asrcStateParams->fracOutputsRemaining);

   /* Use rho directly without storing it in pKerPrivArgs->rho */
   double preCompOutputCnt = ((double) inputSampleCount / rho) + asrcStateParams->fracOutputsRemaining;
   /* This condition check has to be done to prevent the algorithm from interpolating the last output sample without an
    * full input sample.
    * Ex:- for 32 kHz -> 48 kHz example,
    *      Assume input sample count = 256
    *      so precomputer output sample count = 256 * 48/32 = 384 samples
    *      But if you try to generate 384 samples in the algorithm, it'll interpolate the last output sample without an
    *      input sample because it has run out of all 256 input samples. This 384th output sample perfectly aligns with
    *      the 257th input sample (if we have one!). So if this perfect alignment happens we need to reduce 1 output
    *      sample. Which means fracOutputsRemaining will be 1;
    */

   if (preCompOutputCnt == (int32_t) preCompOutputCnt) {
      preCompOutputCnt--;
      preCompOutputCountInt                 = (int32_t) preCompOutputCnt;
      asrcStateParams->fracOutputsRemaining = 1.0;
   }
   else {
      preCompOutputCountInt = (int32_t) preCompOutputCnt;
      /* Update time for next invocation;
         effectively, tau is "reset" each call;
         "fracOutputsRemaining" is fraction of next output sample
         "available" at end of this input block */
      asrcStateParams->fracOutputsRemaining = preCompOutputCnt - (double) preCompOutputCountInt;
   }

   for (i = 0; i < heightIn; i++) {
      memcpy(pOutLocal + i * widthOut, pFilterRembufLocal + i * filterRembufWidthIn,
             asrcStateParams->remBufCount * sizeof(float));
   }
   pOutLocal += asrcStateParams->remBufCount;

   const int32_t rhoInt  = (int32_t) ROUND((double) AUDIOLIB_ASRC_FIXEDPOINT_Q28 *
                                           (double) (rho)); /* output time incr. w.r.t. input sample */
   double        tauTemp = (double) AUDIOLIB_ASRC_FIXEDPOINT_Q28 * (double) (tau);
   int32_t       tauInt  = (int32_t) ROUND(tauTemp); /* output time w.r.t. input sample */
   uint32_t      inputSampleIncrement =
       (uint32_t) (((uint32_t) tauInt & AUDIOLIB_ASRC_INPUT_INCREMENT_MASK) >> AUDIOLIB_ASRC_INPUT_INCREMENT_SHIFT);
   uint32_t nearestInputSampleIndex = inputSampleIncrement;

#pragma MUST_ITERATE(1, , 1)
   for (outSampleIdx = 0; outSampleIdx < preCompOutputCountInt; outSampleIdx++) {
      k                 = (uint32_t) (((uint32_t) tauInt & AUDIOLIB_ASRC_PHASE_MASK) >>
                      AUDIOLIB_ASRC_PHASE_SHIFT); /* upsampler phase */
      const dataType dt = (dataType) (((uint32_t) tauInt & AUDIOLIB_ASRC_FRACTION_MASK)) * AUDIOLIB_ASRC_FRACTION_SCALE;
      const dataType dt2 = dt * dt;
      const dataType dt3 = dt * dt2;
      /* 3rd-order Lagrange interpolation coefficients */
      /* for use with 0 <= dt < 1
       clang-format off*/
      const dataType q30 = (-1.f / 3) * dt + 0.5f * dt2 + (-1.f / 6) * dt3;
      const dataType q31 = 1.f - 0.5f * dt - dt2 + 0.5f * dt3;
      const dataType q32 = dt + 0.5f * dt2 - 0.5f * dt3;
      const dataType q33 = (-1.f / 6) * dt + (1.f / 6) * dt3;
      /* clang-format on */

#pragma MUST_ITERATE(1, , 1)
      for (i = 0; i < heightIn; i++) {
         *(pOutLocal + (i * widthOut)) = 0.0f;
      }
      k *= AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS;

#pragma MUST_ITERATE(64, 64, )
      for (i = 0; i < (int32_t) AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS; i++) {
         /* compute coefficients
          clang-format off */
         hq = q30 * pFiltCoeffsLocal[k] + q31 * pFiltCoeffsLocal[k + 1 * AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS] +
              q32 * pFiltCoeffsLocal[k + 2 * AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS] +
              q33 * pFiltCoeffsLocal[k + 3 * AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS];
         k++;

         /*compute sums */
         j = (*cirBuffStartIndex + nearestInputSampleIndex + i) & (inBufferTotalDimX - 1);
#pragma MUST_ITERATE(1, , 1)
         for (n = 0; n < heightIn; n++) {
            *(pOutLocal + (n * widthOut)) += hq * pInLocal[j + (n * inBufferTotalDimX)];
         }
      }
#pragma MUST_ITERATE(1, , 1)
      for (i = 0; i < heightIn; i++) {
         *(pOutLocal + (i * widthOut)) *= (dataType) AUDIOLIB_ASRC_NUMBER_OF_FILTER_PHASES;
      }
      pOutLocal++;

      tauInt -= (int32_t) (AUDIOLIB_ASRC_FIXEDPOINT_Q28 * inputSampleIncrement);
      tauInt += rhoInt;
      inputSampleIncrement =
          (uint32_t) (((uint32_t) tauInt & AUDIOLIB_ASRC_INPUT_INCREMENT_MASK) >> AUDIOLIB_ASRC_INPUT_INCREMENT_SHIFT);
      nearestInputSampleIndex += inputSampleIncrement;
   }
   *cirBuffStartIndex = (*cirBuffStartIndex + inputSampleCount) & (inBufferTotalDimX - 1);

   /*ASRC frame length must be a multiple of frameModuloFactor*/
   asrcStateParams->outputSampleCount = preCompOutputCountInt + asrcStateParams->remBufCount;
   asrcStateParams->remBufCount       = asrcStateParams->outputSampleCount % frameModuloFactor;
   asrcStateParams->outputSampleCount = asrcStateParams->outputSampleCount - asrcStateParams->remBufCount;

   for (i = 0; i < numChannels; i++) {
      memcpy(pFilterRembufLocal + i * filterRembufWidthIn,
             (dataType *) pOut + asrcStateParams->outputSampleCount + i * widthOut,
             asrcStateParams->remBufCount * AUDIOLIB_sizeof(AUDIOLIB_FLOAT32));
   }
   return;
}

template <typename dataType>
void AUDIOLIB_asrc_exec_cn_interleaved(void                 *pIn,
                                       void                 *pNonInterleavedData,
                                       void                 *pFiltCoeffs,
                                       void                 *pFilterRembuf,
                                       void                 *pOut,
                                       size_t                widthOut,
                                       size_t                heightIn,
                                       TISP_asrcStateParams *asrcStateParams,
                                       uint32_t              inputSampleCnt,
                                       double                maxOutputsPerInputRatio,
                                       size_t                strideInElements,
                                       size_t                strideOutElements,
                                       size_t                isInterleaved,
                                       size_t                filterRembufWidthIn,
                                       uint32_t              inBufferTotalDimX,
                                       size_t                frameModuloFactor,
                                       uint32_t              data_Type)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocal                 = (dataType *) pIn;
   dataType *restrict pOutLocal                = (dataType *) pOut;
   dataType *restrict pNonInterleavedDataLocal = (dataType *) pNonInterleavedData;
   dataType       *pFilterRembufLocal          = (dataType *) pFilterRembuf;
   const dataType *pFiltCoeffsLocal            = (dataType *) pFiltCoeffs;
   const uint8_t   numChannels                 = heightIn;
   dataType        hq;
   uint32_t       *cirBuffStartIndex = &asrcStateParams->cirBuffStartIdx;
   uint32_t        inputSampleCount  = inputSampleCnt;
   int32_t         preCompOutputCountInt;
   int32_t         j, k, outSampleIdx;
   size_t          i, n;

   dataType *pOutLocalT =
       &pNonInterleavedDataLocal[(*cirBuffStartIndex + (uint32_t) (AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS - 1)) &
                                 (inBufferTotalDimX - 1)];

   /* output / input time step
    maxOutputsPerInputRatio will set the maximum rate ratio change acceptable*/
   /** Accumulator increment value. This variable stores the accumulator increment value, which is calculated as
    * 1/asrcRatio.
    */
   const double rho = 1 / AUDIOLIB_ASRC_MIN(asrcStateParams->outputsPerInputRatio, maxOutputsPerInputRatio);
   /** Accumulator value. This variable stores the accumulator value used in the ARC2 core processing.
    */
   const double tau = rho * (1 - asrcStateParams->fracOutputsRemaining);

   /* Use rho directly without storing it in pKerPrivArgs->rho */
   double preCompOutputCnt = ((double) inputSampleCount / rho) + asrcStateParams->fracOutputsRemaining;
   /* This condition check has to be done to prevent the algorithm from interpolating the last output sample without an
    * full input sample.
    * Ex:- for 32 kHz -> 48 kHz example,
    *      Assume input sample count = 256
    *      so precomputer output sample count = 256 * 48/32 = 384 samples
    *      But if you try to generate 384 samples in the algorithm, it'll interpolate the last output sample without an
    *      input sample because it has run out of all 256 input samples. This 384th output sample perfectly aligns with
    *      the 257th input sample (if we have one!). So if this perfect alignment happens we need to reduce 1 output
    *      sample. Which means fracOutputsRemaining will be 1;
    */
   if (preCompOutputCnt == (int32_t) preCompOutputCnt) {
      preCompOutputCnt--;
      preCompOutputCountInt                 = (int32_t) preCompOutputCnt;
      asrcStateParams->fracOutputsRemaining = 1.0;
   }
   else {
      preCompOutputCountInt = (int32_t) preCompOutputCnt;
      /* Update time for next invocation;
         effectively, tau is "reset" each call;
         "fracOutputsRemaining" is fraction of next output sample
         "available" at end of this input block */
      asrcStateParams->fracOutputsRemaining = preCompOutputCnt - (double) preCompOutputCountInt;
   }

   memcpy(pOutLocal, pFilterRembufLocal,
          numChannels * asrcStateParams->remBufCount * AUDIOLIB_sizeof(AUDIOLIB_FLOAT32));

   pOutLocal += asrcStateParams->remBufCount * (int32_t) numChannels;

   const int32_t rhoInt  = (int32_t) ROUND((double) AUDIOLIB_ASRC_FIXEDPOINT_Q28 *
                                           (double) (rho)); /* output time incr. w.r.t. input sample */
   double        tauTemp = (double) AUDIOLIB_ASRC_FIXEDPOINT_Q28 * (double) (tau);
   int32_t       tauInt  = (int32_t) ROUND(tauTemp); /* output time w.r.t. input sample */
   uint32_t      inputSampleIncrement =
       (uint32_t) (((uint32_t) tauInt & AUDIOLIB_ASRC_INPUT_INCREMENT_MASK) >> AUDIOLIB_ASRC_INPUT_INCREMENT_SHIFT);
   uint32_t nearestInputSampleIndex = inputSampleIncrement;
   uint32_t matStride               = inBufferTotalDimX;

   for (i = 0; i < heightIn; i++) {
      for (j = 0; j < (int32_t) inputSampleCount; j++) {
         pOutLocalT[(i * matStride) + j] = pInLocal[i + ((strideInElements) *j)];
      }
   }

#pragma MUST_ITERATE(1, , 1)
   for (outSampleIdx = 0; outSampleIdx < preCompOutputCountInt; outSampleIdx++) {

      k                 = (uint32_t) (((uint32_t) tauInt & AUDIOLIB_ASRC_PHASE_MASK) >>
                      AUDIOLIB_ASRC_PHASE_SHIFT); /* upsampler phase */
      const dataType dt = (dataType) (((uint32_t) tauInt & AUDIOLIB_ASRC_FRACTION_MASK)) * AUDIOLIB_ASRC_FRACTION_SCALE;
      const dataType dt2 = dt * dt;
      const dataType dt3 = dt * dt2;
      /* 3rd-order Lagrange interpolation coefficients */
      /* for use with 0 <= dt < 1 */
      /*  clang-format off */
      const dataType q30 = (-1.f / 3) * dt + 0.5f * dt2 + (-1.f / 6) * dt3;
      const dataType q31 = 1.f - 0.5f * dt - dt2 + 0.5f * dt3;
      const dataType q32 = dt + 0.5f * dt2 - 0.5f * dt3;
      const dataType q33 = (-1.f / 6) * dt + (1.f / 6) * dt3;
      /* clang-format on */

#pragma MUST_ITERATE(1, , 1)
      for (i = 0; i < heightIn; i++) {
         *(pOutLocal + i) = 0.0f;
      }

      k *= AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS;

#pragma MUST_ITERATE(64, 64, )
      for (i = 0; i < (int32_t) AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS; i++) {
         /* compute coefficients */
         /*  clang-format off */
         hq = q30 * pFiltCoeffsLocal[k] + q31 * pFiltCoeffsLocal[k + 1 * AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS] +
              q32 * pFiltCoeffsLocal[k + 2 * AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS] +
              q33 * pFiltCoeffsLocal[k + 3 * AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS];
         /* clang-format on */
         k++;

         /*  compute sums */
         j = (*cirBuffStartIndex + nearestInputSampleIndex + i) & (inBufferTotalDimX - 1);

#pragma MUST_ITERATE(1, , 1)
         for (n = 0; n < heightIn; n++) {
            *(pOutLocal + n) += hq * pNonInterleavedDataLocal[j + (n * inBufferTotalDimX)];
         }
      }

#pragma MUST_ITERATE(1, , 1)
      for (i = 0; i < heightIn; i++) {
         *(pOutLocal + i) *= (dataType) AUDIOLIB_ASRC_NUMBER_OF_FILTER_PHASES;
      }
      pOutLocal += heightIn;

      tauInt -= (int32_t) (AUDIOLIB_ASRC_FIXEDPOINT_Q28 * inputSampleIncrement);
      tauInt += rhoInt;
      inputSampleIncrement =
          (uint32_t) (((uint32_t) tauInt & AUDIOLIB_ASRC_INPUT_INCREMENT_MASK) >> AUDIOLIB_ASRC_INPUT_INCREMENT_SHIFT);
      nearestInputSampleIndex += inputSampleIncrement;
   }

   *cirBuffStartIndex = (*cirBuffStartIndex + inputSampleCount) & (inBufferTotalDimX - 1);

   /* ASRC frame length must be a multiple of frameModuloFactor */
   asrcStateParams->outputSampleCount = preCompOutputCountInt + asrcStateParams->remBufCount;
   asrcStateParams->remBufCount       = asrcStateParams->outputSampleCount % frameModuloFactor;
   asrcStateParams->outputSampleCount = asrcStateParams->outputSampleCount - asrcStateParams->remBufCount;

   memcpy(pFilterRembufLocal, (dataType *) pOut + asrcStateParams->outputSampleCount * numChannels,
          numChannels * asrcStateParams->remBufCount * AUDIOLIB_sizeof(AUDIOLIB_FLOAT32));

   return;
}

template void AUDIOLIB_asrc_exec_cn_non_interleaved<float>(void                 *pIn,
                                                           void                 *pNonInterleavedData,
                                                           void                 *pFiltCoeffs,
                                                           void                 *pFilterRembuf,
                                                           void                 *pOut,
                                                           size_t                widthOut,
                                                           size_t                heightIn,
                                                           TISP_asrcStateParams *asrcStateParams,
                                                           uint32_t              inputSampleCnt,
                                                           double                maxOutputsPerInputRatio,
                                                           size_t                strideInElements,
                                                           size_t                strideOutElements,
                                                           size_t                isInterleaved,
                                                           size_t                filterRembufWidthIn,
                                                           uint32_t              inBufferTotalDimX,
                                                           size_t                frameModuloFactor);
template void AUDIOLIB_asrc_exec_cn_interleaved<float>(void                 *pIn,
                                                       void                 *pNonInterleavedData,
                                                       void                 *pFiltCoeffs,
                                                       void                 *pFilterRembuf,
                                                       void                 *pOut,
                                                       size_t                widthOut,
                                                       size_t                heightIn,
                                                       TISP_asrcStateParams *asrcStateParams,
                                                       uint32_t              inputSampleCnt,
                                                       double                maxOutputsPerInputRatio,
                                                       size_t                strideInElements,
                                                       size_t                strideOutElements,
                                                       size_t                isInterleaved,
                                                       size_t                filterRembufWidthIn,
                                                       uint32_t              inBufferTotalDimX,
                                                       size_t                frameModuloFactor,
                                                       uint32_t              data_Type);

void AUDIOLIB_asrc_cn(void                 *pIn,
                      void                 *pNonInterleavedData,
                      void                 *pFiltCoeffs,
                      void                 *pFilterRembuf,
                      void                 *pOut,
                      size_t                widthOut,
                      size_t                heightIn,
                      TISP_asrcStateParams *asrcStateParams,
                      uint32_t              inputSampleCnt,
                      double                maxOutputsPerInputRatio,
                      size_t                strideInElements,
                      size_t                strideOutElements,
                      size_t                isInterleaved,
                      size_t                filterRembufWidthIn,
                      uint32_t              inBufferTotalDimX,
                      size_t                frameModuloFactor,
                      uint32_t              data_type)
{
   if (isInterleaved == 1) {
      AUDIOLIB_asrc_exec_cn_interleaved<float>(pIn, pNonInterleavedData, pFiltCoeffs, pFilterRembuf, pOut, widthOut,
                                               heightIn, asrcStateParams, inputSampleCnt, maxOutputsPerInputRatio,
                                               strideInElements, strideOutElements, isInterleaved, filterRembufWidthIn,
                                               inBufferTotalDimX, frameModuloFactor, data_type);
   }
   else {
      AUDIOLIB_asrc_exec_cn_non_interleaved<float>(pIn, pNonInterleavedData, pFiltCoeffs, pFilterRembuf, pOut, widthOut,
                                                   heightIn, asrcStateParams, inputSampleCnt, maxOutputsPerInputRatio,
                                                   strideInElements, strideOutElements, isInterleaved,
                                                   filterRembufWidthIn, inBufferTotalDimX, frameModuloFactor);
   }
}
