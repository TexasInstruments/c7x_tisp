// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include "TISP_downwardExpander_cn.h"
#include "TISP_downwardExpander_dB20_cn.h"
#include "TISP_downwardExpander_undB20_cn.h"

template <typename dataType> void TISP_downwardExpander_init_cn(void *pGainState, void *pHoldState, size_t numChannels)
{
   // Mirrors DownwardExpander::cleanInternalBuffers(): the envelope and hold counter start from rest.
   if (numChannels > 0) {
      if (pGainState != nullptr) {
         std::memset(pGainState, 0, numChannels * sizeof(dataType));
      }
      if (pHoldState != nullptr) {
         std::memset(pHoldState, 0, numChannels * sizeof(dataType));
      }
   }

   return;
}

template <typename dataType>
void TISP_downwardExpander_exec_cn(void  *pIn,
                           void  *pGainState,
                           void  *pHoldState,
                           void  *pScratch,
                           void  *pOut,
                           void  *pThreshold_dB,
                           void  *pRatio,
                           void  *pKneeWidth_dB,
                           void  *pAttackCoeff,
                           void  *pReleaseCoeff,
                           void  *pHoldSamples,
                           size_t numChannels,
                           size_t blockSize,
                           size_t pitchX,
                           size_t pitchS,
                           size_t pitchY)
{

   // Localize buffers
   dataType *pInLocal        = (dataType *) pIn;
   dataType *pGainStateLocal = (dataType *) pGainState;
   dataType *pHoldStateLocal = (dataType *) pHoldState;
   dataType *pOutLocal       = (dataType *) pOut;
   dataType *pScratchLocal   = (dataType *) pScratch;

   dataType *pThresholdLocal    = (dataType *) pThreshold_dB;
   dataType *pRatioLocal        = (dataType *) pRatio;
   dataType *pKneeWidthLocal    = (dataType *) pKneeWidth_dB;
   dataType *pAttackCoeffLocal  = (dataType *) pAttackCoeff;
   dataType *pReleaseCoeffLocal = (dataType *) pReleaseCoeff;
   dataType *pHoldSamplesLocal  = (dataType *) pHoldSamples;

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
   TISP_downwardExpander_dB20_exec_cn<float>(pOutLocal, pScratchLocal, numChannels, blockSize, strideElemY, strideElemS);

   // Iterate through all channels
   for (uint32_t ch = 0; ch < numChannels; ch++) {

      // Extract per channel parameters
      dataType threshold_dB_ch = pThresholdLocal[ch];
      dataType ratio_ch        = pRatioLocal[ch];
      dataType kneeWidth_dB_ch = pKneeWidthLocal[ch];
      dataType attackCoeff_ch  = pAttackCoeffLocal[ch];
      dataType releaseCoeff_ch = pReleaseCoeffLocal[ch];
      dataType holdSamples_ch  = pHoldSamplesLocal[ch];

      dataType attackCoeffComplement_ch  = 1 - attackCoeff_ch;
      dataType releaseCoeffComplement_ch = 1 - releaseCoeff_ch;
      dataType releaseFloor_ch           = (releaseCoeff_ch > 0) ? -FLT_MIN : 0;

      dataType smoothedGain_dB_ch = pGainStateLocal[ch];
      dataType holdCount_ch       = pHoldStateLocal[ch];
      dataType x_dB;
      dataType targetGain_dB;

      /* Loop-invariant: these depend only on this channel's parameters. Hoisted out of the
       * sample loop, matching the kernel, which computes them once per channel block. The
       * reciprocal is guarded because the hoist moves it outside the soft-knee branch, where
       * kneeWidth can be zero. */
      dataType halfBandKneeWidth_dB_ch = kneeWidth_dB_ch / 2;
      dataType kneeStart_dB            = threshold_dB_ch - halfBandKneeWidth_dB_ch;
      dataType kneeEnd_dB              = threshold_dB_ch + halfBandKneeWidth_dB_ch;
      dataType invDoubleBandKneeWidth  = (kneeWidth_dB_ch > 0) ? (1 / (2 * kneeWidth_dB_ch)) : 0;

      // Iterate through samples per channel
      for (uint32_t n = 0; n < blockSize; n++) {

         x_dB = pScratchLocal[n * strideElemS + ch];

         /* MATLAB floors the level at 20*log10(DBL_EPSILON) = -313.0712 dB (the kernel does the same) */
         if (x_dB < -313.0712) {

            x_dB = -313.0712;
         }

         /* The computed gain x_sc - x is formed directly from each region of the static
          * characteristic, as the kernel does. Forming x_sc first and subtracting x cancels
          * catastrophically in float32: just inside the knee end the true gain is far below
          * one ulp of x, so x_sc - x rounds to exactly 0 and flips the hold counter. */

         // Hard knee
         if (kneeWidth_dB_ch <= 0) {

            if (x_dB < threshold_dB_ch) {

               // x_sc = T + (x - T) * R
               targetGain_dB = (x_dB - threshold_dB_ch) * (ratio_ch - 1);
            }

            else {

               // x_sc = x
               targetGain_dB = 0;
            }
         }

         // Soft knee
         else {

            if (x_dB < kneeStart_dB) {

               // x_sc = T + (x - T) * R
               targetGain_dB = (x_dB - threshold_dB_ch) * (ratio_ch - 1);
            }

            else if (x_dB > kneeEnd_dB) {

               // x_sc = x
               targetGain_dB = 0;
            }

            else {

               // x_sc = x + (1 - R) * (x - T - W/2)^2 / (2W)
               dataType R  = x_dB - kneeEnd_dB;
               dataType R2 = R * R;

               targetGain_dB = (1 - ratio_ch) * R2 * invDoubleBandKneeWidth;
            }
         }

         // Release mode
         if (targetGain_dB > smoothedGain_dB_ch) {

            holdCount_ch       = 0;
            /* A release toward 0 dB stops at -FLT_MIN instead of flushing to 0, so the next 0 dB target resets
             * the hold counter */
            smoothedGain_dB_ch = releaseCoeff_ch * smoothedGain_dB_ch +
                                 (releaseCoeffComplement_ch * targetGain_dB + releaseFloor_ch);
         }

         // Attack mode
         else if (targetGain_dB < smoothedGain_dB_ch) {

            holdCount_ch += 1;

            // Hold previous gain until hold time has elapsed
            if (holdCount_ch > holdSamples_ch) {

               smoothedGain_dB_ch = attackCoeff_ch * smoothedGain_dB_ch + attackCoeffComplement_ch * targetGain_dB;
            }
         }

         // Gain unchanged: the hold counter stays as it is (measured MATLAB rule)
         else {

            /* Nothing to do here */
         }

         pOutLocal[n * strideElemY + ch] = smoothedGain_dB_ch;
      }

      // Carry this block's final envelope and hold counter to the next exec().
      pGainStateLocal[ch] = smoothedGain_dB_ch;
      pHoldStateLocal[ch] = holdCount_ch;
   }

   // Convert to linear scale
   TISP_downwardExpander_undB20_exec_cn<float>(pOutLocal, pScratchLocal, numChannels, blockSize, strideElemY, strideElemS);

   // Apply linear gain to input
   for (uint32_t ch = 0; ch < numChannels; ch++) {
      for (uint32_t n = 0; n < blockSize; n++) {

         pOutLocal[n * strideElemY + ch] = pInLocal[n * strideElemX + ch] * pScratchLocal[n * strideElemS + ch];
      }
   }

   return;
}

// explicit instantiation for the different data type versions
template void TISP_downwardExpander_init_cn<float>(void *pGainState, void *pHoldState, size_t numChannels);

template void TISP_downwardExpander_exec_cn<float>(void  *pIn,
                                           void  *pGainState,
                                           void  *pHoldState,
                                           void  *pScratch,
                                           void  *pOut,
                                           void  *pThreshold_dB,
                                           void  *pRatio,
                                           void  *pKneeWidth_dB,
                                           void  *pAttackCoeff,
                                           void  *pReleaseCoeff,
                                           void  *pHoldSamples,
                                           size_t numChannels,
                                           size_t blockSize,
                                           size_t pitchX,
                                           size_t pitchS,
                                           size_t pitchY);
