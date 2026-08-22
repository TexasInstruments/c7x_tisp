// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <algorithm>
#include <cmath>
#include <cstring>

#include "TISP_limiter_cn.h"
#include "TISP_limiter_dB20_cn.h"
#include "TISP_limiter_undB20_cn.h"

template <typename dataType> void TISP_limiter_init_cn(void *pGainState, size_t numChannels)
{
   // Mirrors Limiter::cleanInternalBuffers(): the envelope starts from rest.
   if (pGainState != nullptr && numChannels > 0) {
      std::memset(pGainState, 0, numChannels * sizeof(dataType));
   }

   return;
}

template <typename dataType>
void TISP_limiter_exec_cn(void  *pIn,
                          void  *pGainState,
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
   dataType *pInLocal        = (dataType *) pIn; // Interleaved
   dataType *pGainStateLocal = (dataType *) pGainState;
   dataType *pOutLocal       = (dataType *) pOut; // Interleaved
   dataType *pScratchLocal   = (dataType *) pScratch;

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
      dataType threshold_dB_ch = pThresholdLocal[ch];
      dataType kneeWidth_dB_ch = pKneeWidthLocal[ch];
      dataType attackCoeff_ch  = pAttackCoeffLocal[ch];
      dataType releaseCoeff_ch = pReleaseCoeffLocal[ch];

      dataType attackCoeffComplement_ch  = 1 - attackCoeff_ch;
      dataType releaseCoeffComplement_ch = 1 - releaseCoeff_ch;

      dataType smoothedGain_dB_ch = pGainStateLocal[ch];
      dataType x_dB, x_sc;
      dataType targetGain_dB;

      dataType halfBandKneeWidth_dB_ch = kneeWidth_dB_ch / 2;
      dataType x_sc_at_0dB;

      // Hard knee
      if (kneeWidth_dB_ch <= 0) {

         if (0 >= threshold_dB_ch) {

            x_sc_at_0dB = threshold_dB_ch;
         }

         else {

            x_sc_at_0dB = 0;
         }
      }

      // Soft knee
      else {

         if (0 < threshold_dB_ch - halfBandKneeWidth_dB_ch) {

            x_sc_at_0dB = 0;
         }

         else if (0 > threshold_dB_ch + halfBandKneeWidth_dB_ch) {

            x_sc_at_0dB = threshold_dB_ch;
         }

         else {

            dataType RAtFullScale = 0 - threshold_dB_ch + halfBandKneeWidth_dB_ch;

            x_sc_at_0dB = 0 - (RAtFullScale * RAtFullScale) / (2 * kneeWidth_dB_ch);
         }
      }

      dataType makeUpGain_dB_ch = -x_sc_at_0dB;

      /* Loop-invariant: these depend only on this channel's parameters. Hoisted out of the
       * sample loop, matching the kernel, which computes them once per channel block. The
       * reciprocal is guarded because the hoist moves it outside the soft-knee branch, where
       * kneeWidth can be zero. */
      dataType kneeStart_dB           = threshold_dB_ch - halfBandKneeWidth_dB_ch;
      dataType kneeEnd_dB             = threshold_dB_ch + halfBandKneeWidth_dB_ch;
      dataType invDoubleBandKneeWidth = (kneeWidth_dB_ch > 0) ? (1 / (2 * kneeWidth_dB_ch)) : 0;

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

            if (x_dB < kneeStart_dB) {

               x_sc = x_dB;
            }

            else if (x_dB > kneeEnd_dB) {

               x_sc = threshold_dB_ch;
            }

            else {

               dataType R  = x_dB - threshold_dB_ch + halfBandKneeWidth_dB_ch;
               dataType R2 = R * R;

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

         // Apply the constant automatic make-up gain
         dataType gain_makeUp = smoothedGain_dB_ch + makeUpGain_dB_ch;

         pOutLocal[n * strideElemY + ch] = gain_makeUp;
      }

      // Carry this block's final envelope to the next exec().
      pGainStateLocal[ch] = smoothedGain_dB_ch;
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
template void TISP_limiter_init_cn<float>(void *pGainState, size_t numChannels);

template void TISP_limiter_exec_cn<float>(void  *pIn,
                                          void  *pGainState,
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
                                          bool   isInterleaved);
