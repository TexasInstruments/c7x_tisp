// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_ssrc_cn.h"
#include "AUDIOLIB_ssrc/AUDIOLIB_ssrc_priv.h"
#include "common/AUDIOLIB_bufParams.h"
#include <algorithm>
#include <audiolib.h>
#include <cstddef>
#include <dsplib.h>
#include <math.h>
#include <stdexcept>

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

bool AUDIOLIB_ssrc_cn_set(void    *pIn,
                          void    *pState,
                          uint8_t  mode,
                          uint8_t  inputSampleRate,
                          uint8_t  outputSampleRate,
                          uint32_t numChannels,
                          uint32_t inBufferTotalDimX,
                          uint32_t sampleRateRatio,
                          int32_t  inputSampleCount,
                          uint8_t  bufferFormat,
                          uint32_t sampleDataType)
{
   int32_t  eleSize;
   uint32_t historyLengthStage1;
   uint32_t historyLengthStage2;
   int32_t  historyStartOffset;
   int32_t  i;
   eleSize                  = AUDIOLIB_sizeof(sampleDataType);
   int32_t  alignedEleCount = AUDIOLIB_L2DATA_ALIGNMENT / eleSize;
   uint32_t stage1DataBufferEleCount;
   bool     status = true;
   if (pIn == nullptr) {
      status = false;
   }
   else if (bufferFormat != 1 && (pState == nullptr)) {
      status = false;
   }
   else if (mode != (uint8_t) AUDIOLIB_SSRC_MODE_RESET) {
      status = false;
   }
   else {
      status = true;
   }
   historyLengthStage1 = AUDIOLIB_ssrc_getSampleHistoryLength((ssrc_sample_rate_t) inputSampleRate,
                                                              (ssrc_sample_rate_t) outputSampleRate, 1U);
   historyLengthStage2 = AUDIOLIB_ssrc_getSampleHistoryLength((ssrc_sample_rate_t) inputSampleRate,
                                                              (ssrc_sample_rate_t) outputSampleRate, 2U);
   if (bufferFormat == 1) {
      /* For circular buffer format in the non interleaved mode, there is  either one or two circular
      buffers with interleaved channel data Calculate the starting position for the filter history in
      the circular buffer */
      historyStartOffset = inBufferTotalDimX - historyLengthStage1 * numChannels;
      /* In interleaved mode, we need to clear the filter history for all channels */
      float *pInLocal = (float *) pIn + historyStartOffset;
      /* Clear stage 1 filter history */
      memset((void *) pInLocal, 0, historyLengthStage1 * numChannels * eleSize);

      /* If 4x conversion, also clear stage 2 filter history */
      if (sampleRateRatio == 4) {
         historyStartOffset = inBufferTotalDimX * 2 - historyLengthStage2 * numChannels;
         pInLocal           = (float *) pIn + historyStartOffset;
         memset((void *) pInLocal, 0, historyLengthStage2 * numChannels * eleSize);
         historyStartOffset = inBufferTotalDimX;
         pInLocal           = (float *) pIn + historyStartOffset;
         memset((void *) pInLocal, 0, historyLengthStage2 * numChannels * eleSize);
      }
   }
   else {
      /* For linear buffer format */

      /* For linear buffer format, we use a separate state buffer */
      /* Clear stage 1 filter history */
      float *pStateLocal = (float *) pState;
      for (i = 0; i < (int32_t) numChannels; i++) {
         memset((void *) pStateLocal, 0, historyLengthStage1 * eleSize);
         pStateLocal += inBufferTotalDimX;
      }

      /* If 4x conversion, also clear stage 2 filter history */
      stage1DataBufferEleCount =
          AUDIOLIB_ceilingDiv(historyLengthStage1 + inputSampleCount, alignedEleCount) * alignedEleCount;
      if (sampleRateRatio == 4) {
         pStateLocal = (float *) pState + stage1DataBufferEleCount;
         for (i = 0; i < (int32_t) numChannels; i++) {
            memset((void *) pStateLocal, 0, historyLengthStage2 * eleSize);
            pStateLocal += inBufferTotalDimX;
         }
      }
   }

   return status;
}

template <typename dataType>
void AUDIOLIB_ssrc_upsample2x_exec_cn(void                 *pIn,
                                      void                 *pFiltCoeffs,
                                      void                 *pOut,
                                      size_t                heightIn,
                                      uint32_t              cirBuffAddressMask,
                                      int32_t               inputSampleCount,
                                      TISP_ssrcStateParams *ssrcStateParams)
{
   /* Typecast void pointers to respective data type */
   dataType       *pInLocal     = (dataType *) pIn;
   dataType       *pOutLocal    = (dataType *) pOut;
   const dataType *pFilterLocal = (dataType *) pFiltCoeffs;

   int32_t numChannels = heightIn;
   int32_t i, j, k;
   for (i = 0; i < inputSampleCount; i++) {
      for (j = 0; j < numChannels; j++) {
         dataType outTemp       = 0;
         uint32_t dataOffset    = (ssrcStateParams->stage1BuffStartIndex + i * numChannels + j) & cirBuffAddressMask;
         int32_t  dataOffsetOut = i * (int32_t) AUDIOLIB_SSRC_NUMBER_OF_FILTER_PHASES * numChannels + j;

         uint32_t idx1, idx2;
         for (k = 0; k < static_cast<int32_t>(AUDIOLIB_SSRC_UPSAMPLING_STAGE1_FILTER_TAPS); k++) {
            idx1 = (dataOffset + k * numChannels) & cirBuffAddressMask;
            idx2 = (dataOffset + ((AUDIOLIB_SSRC_UPSAMPLING_STAGE1_FILTER_TAPS_SYMMETRIC - 1) - k) * numChannels) &
                   cirBuffAddressMask;

            dataType inVal1    = pInLocal[idx1];
            dataType inVal2    = pInLocal[idx2];
            dataType filterVal = pFilterLocal[k];
            dataType outProd   = (inVal1 + inVal2) * filterVal;

            outTemp += outProd;
         }

         idx1                                   = (dataOffset + k * numChannels) & cirBuffAddressMask;
         pOutLocal[dataOffsetOut]               = outTemp;
         pOutLocal[dataOffsetOut + numChannels] = pInLocal[idx1];
      }
   }

   ssrcStateParams->stage1BuffStartIndex =
       (uint32_t) (ssrcStateParams->stage1BuffStartIndex + inputSampleCount * numChannels) & cirBuffAddressMask;
}

template <typename dataType>
void AUDIOLIB_ssrc_upsample4x_exec_cn(void                 *pIn,
                                      void                 *pFiltCoeffs,
                                      void                 *pOut,
                                      size_t                heightIn,
                                      uint32_t              cirBuffAddressMask,
                                      int32_t               inputSampleCount,
                                      TISP_ssrcStateParams *ssrcStateParams)
{
   /* Typecast void pointers to respective data type */
   dataType       *pInLocal           = (dataType *) pIn;
   dataType       *pOutLocal          = ((dataType *) pOut);
   const dataType *pFilterCoeffsLocal = (dataType *) pFiltCoeffs;
   const dataType *pFilterStage1      = pFilterCoeffsLocal + ssrcStateParams->stage1FiltCoeffsOffset;
   const dataType *pFilterStage2      = pFilterCoeffsLocal + ssrcStateParams->stage2FiltCoeffsOffset;
   int32_t         numChannels        = heightIn;
   int32_t         i, j, k;

   /* For interleaved mode, we use the second half of the pIn buffer as the intermediate buffer
   This is possible because we've allocated double the size for pIn when sampleRateRatio is 4
   as seen in the test driver file */
   dataType *pIntermediateLocal =
       pInLocal + ssrcStateParams->inBufferTotalDimX; /* Point to the second half of pIn buffer */

   /* Stage 1: 2x upsampling using stage1FiltCoeffs */
   for (i = 0; i < inputSampleCount; i++) {
      for (j = 0; j < numChannels; j++) {
         dataType outTemp       = 0;
         uint32_t dataOffset    = (ssrcStateParams->stage1BuffStartIndex + i * numChannels + j) & cirBuffAddressMask;
         uint32_t dataOffsetOut = (ssrcStateParams->stage2BuffCurrIndex +
                                   i * (int32_t) AUDIOLIB_SSRC_NUMBER_OF_FILTER_PHASES * numChannels + j) &
                                  cirBuffAddressMask;

         uint32_t idx1, idx2;
         for (k = 0; k < static_cast<int32_t>(AUDIOLIB_SSRC_UPSAMPLING_STAGE1_FILTER_TAPS); k++) {
            idx1 = (dataOffset + k * numChannels) & cirBuffAddressMask;
            idx2 = (dataOffset + ((AUDIOLIB_SSRC_UPSAMPLING_STAGE1_FILTER_TAPS_SYMMETRIC - 1) - k) * numChannels) &
                   cirBuffAddressMask;

            dataType inVal1    = pInLocal[idx1];
            dataType inVal2    = pInLocal[idx2];
            dataType filterVal = pFilterStage1[k];
            dataType outProd   = (inVal1 + inVal2) * filterVal;

            outTemp += outProd;
         }

         idx1                              = (dataOffset + k * numChannels) & cirBuffAddressMask;
         pIntermediateLocal[dataOffsetOut] = outTemp;
         pIntermediateLocal[(dataOffsetOut + numChannels) & cirBuffAddressMask] = pInLocal[idx1];
      }
   }

   /* Update circular buffer index after first stage */
   ssrcStateParams->stage1BuffStartIndex =
       (uint32_t) (ssrcStateParams->stage1BuffStartIndex + inputSampleCount * numChannels) & cirBuffAddressMask;

   /* Stage 2: 2x upsampling using stage2FiltCoeffs */
   int32_t stage2InputSampleCount = inputSampleCount * 2;
   ssrcStateParams->stage2BuffCurrIndex =
       (uint32_t) (ssrcStateParams->stage2BuffCurrIndex + stage2InputSampleCount * numChannels) & cirBuffAddressMask;
   /* We'll use the pIntermediateLocal (second half of pIn) as our stage2 circular buffer
   The stage2 filter is smaller than stage1, so we have enough space */

   /* Stage 2: 2x upsampling using stage2FiltCoeffs */
   for (i = 0; i < stage2InputSampleCount; i++) {
      for (j = 0; j < numChannels; j++) {
         dataType outTemp       = 0;
         uint32_t dataOffset    = (ssrcStateParams->stage2BuffStartIndex + i * numChannels + j) & cirBuffAddressMask;
         int32_t  dataOffsetOut = i * (int32_t) AUDIOLIB_SSRC_NUMBER_OF_FILTER_PHASES * numChannels + j;

         uint32_t idx1, idx2;
         for (k = 0; k < static_cast<int32_t>(AUDIOLIB_SSRC_UPSAMPLING_STAGE2_FILTER_TAPS); k++) {
            idx1 = (dataOffset + k * numChannels) & cirBuffAddressMask;
            idx2 = (dataOffset + ((AUDIOLIB_SSRC_UPSAMPLING_STAGE2_FILTER_TAPS_SYMMETRIC - 1) - k) * numChannels) &
                   cirBuffAddressMask;
            dataType inVal1    = pIntermediateLocal[idx1];
            dataType inVal2    = pIntermediateLocal[idx2];
            dataType filterVal = pFilterStage2[k];
            dataType outProd   = (inVal1 + inVal2) * filterVal;

            outTemp += outProd;
         }

         idx1                                   = (dataOffset + k * numChannels) & cirBuffAddressMask;
         pOutLocal[dataOffsetOut]               = outTemp;
         pOutLocal[dataOffsetOut + numChannels] = pIntermediateLocal[idx1];
      }
   }

   ssrcStateParams->stage2BuffStartIndex =
       (uint32_t) (ssrcStateParams->stage2BuffStartIndex + stage2InputSampleCount * numChannels) & cirBuffAddressMask;
}

template <typename dataType>
void AUDIOLIB_ssrc_downsample2x_exec_cn(void                 *pIn,
                                        void                 *pFiltCoeffs,
                                        void                 *pOut,
                                        size_t                heightIn,
                                        uint32_t              cirBuffAddressMask,
                                        int32_t               inputSampleCount,
                                        int32_t               outputSampleCount,
                                        TISP_ssrcStateParams *ssrcStateParams)
{
   /* Typecast void pointers to respective data type */
   dataType       *pInLocal     = (dataType *) pIn;
   dataType       *pOutLocal    = (dataType *) pOut;
   const dataType *pFilterLocal = (dataType *) pFiltCoeffs;
   const uint8_t   numChannels  = heightIn;
   int32_t         i, j, k;

   for (i = 0; i < outputSampleCount; i++) {
      for (j = 0; j < numChannels; j++) {
         dataType outTemp    = 0;
         uint32_t dataOffset = (ssrcStateParams->stage1BuffStartIndex + i * 2 * numChannels + j) & cirBuffAddressMask;
         int32_t  dataOffsetOut = i * numChannels + j;

         uint32_t idx1, idx2;
         for (k = 0; k < static_cast<int32_t>(AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_FILTER_TAPS); k++) {
            idx1 = (dataOffset + k * 2 * numChannels) & cirBuffAddressMask;
            idx2 =
                (dataOffset + ((AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_ORIGINAL_FILTER_TAPS - 1) - (k * 2)) * numChannels) &
                cirBuffAddressMask;

            dataType inVal1    = pInLocal[idx1];
            dataType inVal2    = pInLocal[idx2];
            dataType filterVal = pFilterLocal[k];
            dataType outProd   = (inVal1 + inVal2) * filterVal;

            outTemp += outProd;
         }

         idx1                     = (dataOffset + ((k * 2) - 1) * numChannels) & cirBuffAddressMask;
         pOutLocal[dataOffsetOut] = outTemp + pInLocal[idx1] * 0.5f;
      }
   }

   ssrcStateParams->stage1BuffStartIndex =
       (uint32_t) (ssrcStateParams->stage1BuffStartIndex + inputSampleCount * numChannels) & cirBuffAddressMask;
}

template <typename dataType>
void AUDIOLIB_ssrc_downsample4x_exec_cn(void                 *pIn,
                                        void                 *pFiltCoeffs,
                                        void                 *pOut,
                                        size_t                heightIn,
                                        uint32_t              cirBuffAddressMask,
                                        int32_t               inputSampleCount,
                                        int32_t               outputSampleCount,
                                        TISP_ssrcStateParams *ssrcStateParams)
{
   /* Typecast void pointers to respective data type */
   dataType       *pInLocal           = (dataType *) pIn;
   dataType       *pOutLocal          = ((dataType *) pOut);
   const dataType *pFilterCoeffsLocal = (dataType *) pFiltCoeffs;
   const dataType *pFilterStage1      = pFilterCoeffsLocal + ssrcStateParams->stage1FiltCoeffsOffset;
   const dataType *pFilterStage2      = pFilterCoeffsLocal + ssrcStateParams->stage2FiltCoeffsOffset;
   int32_t         numChannels        = heightIn;
   int32_t         i, j, k;

   /* For interleaved mode, we use the second half of the pIn buffer as the intermediate buffer */
   dataType *pIntermediateLocal =
       pInLocal + ssrcStateParams->inBufferTotalDimX; /* Point to the second half of pIn buffer */

   /* Stage 1: 2x downsampling using stage1FiltCoeffs */
   int32_t stage1OutputSampleCount = inputSampleCount / 2;

   /* Process each output sample for stage 1 */
   for (i = 0; i < stage1OutputSampleCount; i++) {
      for (j = 0; j < numChannels; j++) {
         dataType outTemp = 0;
         /* For each output sample, we process 2 input samples (2x downsampling) */
         uint32_t dataOffset = (ssrcStateParams->stage1BuffStartIndex + i * 2 * numChannels + j) & cirBuffAddressMask;
         uint32_t dataOffsetOut = (ssrcStateParams->stage2BuffCurrIndex + i * numChannels + j) & cirBuffAddressMask;

         /* Apply the filter - using symmetric filter coefficients */
         uint32_t idx1, idx2;
         for (k = 0; k < static_cast<int32_t>(AUDIOLIB_SSRC_DOWNSAMPLING_STAGE1_FILTER_TAPS); k++) {
            idx1 = (dataOffset + k * 2 * numChannels) & cirBuffAddressMask;
            idx2 =
                (dataOffset + ((AUDIOLIB_SSRC_DOWNSAMPLING_STAGE1_ORIGINAL_FILTER_TAPS - 1) - (k * 2)) * numChannels) &
                cirBuffAddressMask;

            dataType inVal1    = pInLocal[idx1];
            dataType inVal2    = pInLocal[idx2];
            dataType filterVal = pFilterStage1[k];
            dataType outProd   = (inVal1 + inVal2) * filterVal;

            outTemp += outProd;
         }

         /* Store the intermediate output sample */
         idx1                              = (dataOffset + ((k * 2) - 1) * numChannels) & cirBuffAddressMask;
         pIntermediateLocal[dataOffsetOut] = outTemp + pInLocal[idx1] * 0.5f;
      }
   }

   /* Update circular buffer indices after first stage */
   ssrcStateParams->stage1BuffStartIndex =
       (uint32_t) (ssrcStateParams->stage1BuffStartIndex + inputSampleCount * numChannels) & cirBuffAddressMask;
   ssrcStateParams->stage2BuffCurrIndex =
       (uint32_t) (ssrcStateParams->stage2BuffCurrIndex + stage1OutputSampleCount * numChannels) & cirBuffAddressMask;

   /* Process each output sample for stage 2 */
   for (i = 0; i < outputSampleCount; i++) {
      for (j = 0; j < numChannels; j++) {
         dataType outTemp = 0;
         /* For each output sample, we process 2 intermediate samples (2x downsampling) */
         uint32_t dataOffset = (ssrcStateParams->stage2BuffStartIndex + i * 2 * numChannels + j) & cirBuffAddressMask;
         int32_t  dataOffsetOut = i * numChannels + j;

         /* Apply the filter - using symmetric filter coefficients */
         uint32_t idx1, idx2;
         for (k = 0; k < static_cast<int32_t>(AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_FILTER_TAPS); k++) {
            idx1 = (dataOffset + k * 2 * numChannels) & cirBuffAddressMask;
            idx2 =
                (dataOffset + ((AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_ORIGINAL_FILTER_TAPS - 1) - (k * 2)) * numChannels) &
                cirBuffAddressMask;

            dataType inVal1    = pIntermediateLocal[idx1];
            dataType inVal2    = pIntermediateLocal[idx2];
            dataType filterVal = pFilterStage2[k];
            dataType outProd   = (inVal1 + inVal2) * filterVal;

            outTemp += outProd;
         }

         /* Store the final output sample */
         idx1                     = (dataOffset + ((k * 2) - 1) * numChannels) & cirBuffAddressMask;
         pOutLocal[dataOffsetOut] = outTemp + pIntermediateLocal[idx1] * 0.5f;
      }
   }

   /* Update circular buffer index for next call */
   ssrcStateParams->stage2BuffStartIndex =
       (uint32_t) (ssrcStateParams->stage2BuffStartIndex + stage1OutputSampleCount * numChannels) & cirBuffAddressMask;
}

template void AUDIOLIB_ssrc_upsample4x_exec_cn<float>(void                 *pIn,
                                                      void                 *pFiltCoeffs,
                                                      void                 *pOut,
                                                      size_t                heightIn,
                                                      uint32_t              cirBuffAddressMask,
                                                      int32_t               inputSampleCount,
                                                      TISP_ssrcStateParams *ssrcStateParams);

template void AUDIOLIB_ssrc_upsample2x_exec_cn<float>(void                 *pIn,
                                                      void                 *pFiltCoeffs,
                                                      void                 *pOut,
                                                      size_t                heightIn,
                                                      uint32_t              cirBuffAddressMask,
                                                      int32_t               inputSampleCount,
                                                      TISP_ssrcStateParams *ssrcStateParams);

template void AUDIOLIB_ssrc_downsample4x_exec_cn<float>(void                 *pIn,
                                                        void                 *pFiltCoeffs,
                                                        void                 *pOut,
                                                        size_t                heightIn,
                                                        uint32_t              cirBuffAddressMask,
                                                        int32_t               inputSampleCount,
                                                        int32_t               outputSampleCount,
                                                        TISP_ssrcStateParams *ssrcStateParams);

template void AUDIOLIB_ssrc_downsample2x_exec_cn<float>(void                 *pIn,
                                                        void                 *pFiltCoeffs,
                                                        void                 *pOut,
                                                        size_t                heightIn,
                                                        uint32_t              cirBuffAddressMask,
                                                        int32_t               inputSampleCount,
                                                        int32_t               outputSampleCount,
                                                        TISP_ssrcStateParams *ssrcStateParams);

void AUDIOLIB_ssrc_cn(void                 *pIn,
                      void                 *pFiltCoeffs,
                      void                 *pOutCn,
                      uint32_t              widthIn,
                      uint32_t              heightIn,
                      uint32_t              cirBuffAddressMask,
                      int32_t               inputSampleCount,
                      bool                  upSampling,
                      uint32_t              samplingRateRatio,
                      int32_t               outputSampleCount,
                      TISP_ssrcStateParams *ssrcStateParams)
{
   if (upSampling) {
      if (samplingRateRatio == 4) {
         AUDIOLIB_ssrc_upsample4x_exec_cn<float>(pIn, pFiltCoeffs, pOutCn, heightIn, cirBuffAddressMask,
                                                 inputSampleCount, ssrcStateParams);
      }
      else {
         AUDIOLIB_ssrc_upsample2x_exec_cn<float>(pIn, pFiltCoeffs, pOutCn, heightIn, cirBuffAddressMask,
                                                 inputSampleCount, ssrcStateParams);
      }
   }
   else {
      if (samplingRateRatio == 4) {
         AUDIOLIB_ssrc_downsample4x_exec_cn<float>(pIn, pFiltCoeffs, pOutCn, heightIn, cirBuffAddressMask,
                                                   inputSampleCount, outputSampleCount, ssrcStateParams);
      }
      else {
         AUDIOLIB_ssrc_downsample2x_exec_cn<float>(pIn, pFiltCoeffs, pOutCn, heightIn, cirBuffAddressMask,
                                                   inputSampleCount, outputSampleCount, ssrcStateParams);
      }
   }
}
