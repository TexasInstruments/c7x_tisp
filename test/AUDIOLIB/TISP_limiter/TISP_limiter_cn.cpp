// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <algorithm>
#include <cmath>

#include "TISP_limiter_cn.h"
#include "TISP_limiter_dB20_cn.h"
#include "TISP_limiter_undB20_cn.h"

template <typename dataType>
void TISP_limiter_exec_cn(void  *pIn,
                          void  *pGain,
                          void  *pScratch,
                          void  *pOut,
                          void  *pThreshold_dB,
                          void  *pKneeWidth_dB,
                          void  *pAttackCoeff,
                          void  *pReleaseCoeff,
                          size_t numChannels,
                          size_t blockSize,
                          size_t pitchX,
                          size_t pitchS,
                          size_t pitchY,
                          bool   isInterleaved)
{

   // Localize buffers
   dataType *pInLocal      = (dataType *) pIn; // Interleaved
   dataType *pGainLocal    = (dataType *) pGain;
   dataType *pOutLocal     = (dataType *) pOut; // Interleaved
   dataType *pScratchLocal = (dataType *) pScratch;

   dataType *pThresholdLocal    = (dataType *) pThreshold_dB;
   dataType *pKneeWidthLocal    = (dataType *) pKneeWidth_dB;
   dataType *pAttackCoeffLocal  = (dataType *) pAttackCoeff;
   dataType *pReleaseCoeffLocal = (dataType *) pReleaseCoeff;

   size_t strideElemX = pitchX / sizeof(dataType);
   size_t strideElemS = pitchS / sizeof(dataType);
   size_t strideElemY = pitchY / sizeof(dataType);

   // Convert to absolute value
   for (uint32_t ch = 0; ch < numChannels; ch++) {
      for (uint32_t n = 0; n < blockSize; n++) {

         pOutLocal[n * strideElemY + ch] = std::abs(pInLocal[n * strideElemX + ch]);
      }
   }

   // Convert to log scale
   TISP_limiter_dB20_exec_cn<float>(pOutLocal, pScratchLocal, numChannels, blockSize, strideElemY, strideElemS);

   // Iterate through all channels
   for (uint32_t ch = 0; ch < numChannels; ch++) {

      // Extract per channel parameters
      dataType threshold_dB_ch = (dataType) pThresholdLocal[ch];
      dataType kneeWidth_dB_ch = (dataType) pKneeWidthLocal[ch];
      dataType attackCoeff_ch  = (dataType) pAttackCoeffLocal[ch];
      dataType releaseCoeff_ch = (dataType) pReleaseCoeffLocal[ch];

      dataType attackCoeffComplement_ch  = 1 - attackCoeff_ch;
      dataType releaseCoeffComplement_ch = 1 - releaseCoeff_ch;

      dataType smoothedGain_dB_ch = 0;
      dataType x_abs, x_dB, x_sc;
      dataType gain_makeUp         = 0;
      dataType smoothedGain_linear = 0;
      dataType targetGain_dB;

      // Iterate through samples per channel
      for (uint32_t n = 0; n < blockSize; n++) {

         x_dB = pScratchLocal[n * strideElemS + ch];

         // Hard knee
         if (kneeWidth_dB_ch <= 0) {

            if (x_dB >= threshold_dB_ch) {

               x_sc = threshold_dB_ch;
            }

            else {

               x_sc = x_dB;
            }
         }

         // Soft knee
         else {

            dataType halfBandKneeWidth_dB = kneeWidth_dB_ch / 2;
            dataType kneeStart_dB         = threshold_dB_ch - halfBandKneeWidth_dB;
            dataType kneeEnd_dB           = threshold_dB_ch + halfBandKneeWidth_dB;

            if (x_dB < kneeStart_dB) {

               x_sc = x_dB;
            }

            else if (x_dB > kneeEnd_dB) {

               x_sc = threshold_dB_ch;
            }

            else {

               dataType R                      = x_dB - threshold_dB_ch + halfBandKneeWidth_dB;
               dataType R2                     = R * R;
               dataType invDoubleBandKneeWidth = 1 / (2 * kneeWidth_dB_ch);

               x_sc = x_dB - R2 * invDoubleBandKneeWidth;
            }
         }

         // Computed gain
         targetGain_dB = x_sc - x_dB;

         // Attack mode
         if (targetGain_dB <= smoothedGain_dB_ch) {

            smoothedGain_dB_ch = attackCoeff_ch * smoothedGain_dB_ch + attackCoeffComplement_ch * targetGain_dB;
         }

         // Release mode
         else {

            smoothedGain_dB_ch = releaseCoeff_ch * smoothedGain_dB_ch + releaseCoeffComplement_ch * targetGain_dB;
         }

         // Apply automatic make-up gain to compensate for 0dB input
         if (x_dB == 0) {

            gain_makeUp = smoothedGain_dB_ch - x_sc;
         }

         else {

            gain_makeUp = smoothedGain_dB_ch;
         }

         pOutLocal[n * strideElemY + ch] = gain_makeUp;
      }

      // Store last gain value per channel
      pGainLocal[ch] = gain_makeUp;
   }

   // Convert to linear scale
   TISP_limiter_undB20_exec_cn<float>(pOutLocal, pScratchLocal, numChannels, blockSize, strideElemY, strideElemS);

   // Apply linear gain to input
   for (uint32_t ch = 0; ch < numChannels; ch++) {
      for (uint32_t n = 0; n < blockSize; n++) {

         pOutLocal[n * strideElemY + ch] = pInLocal[n * strideElemX + ch] * pScratchLocal[n * strideElemS + ch];
      }
   }

   return;
}

// explicit instantiation for the different data type versions
template void TISP_limiter_exec_cn<float>(void  *pIn,
                                          void  *pOut,
                                          void  *pGain,
                                          void  *pScratch,
                                          void  *pThreshold_dB,
                                          void  *pKneeWidth_dB,
                                          void  *pAttackCoeff,
                                          void  *pReleaseCoeff,
                                          size_t numChannels,
                                          size_t blockSize,
                                          size_t pitchX,
                                          size_t pitchS,
                                          size_t pitchY,
                                          bool   isInterleaved);
