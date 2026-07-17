// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_balanceFader_cn.h"
#include "../TISP_gainNCh/TISP_gainNCh_cn.h"
#include <audiolib.h>
#include <math.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

void AUDIOLIB_balanceFader_cn_set(void    *pG,
                                  float    balance,
                                  float    fade,
                                  float    sideGainFactor,
                                  size_t   heightIn,
                                  int32_t *channelConfig,
                                  int32_t  lfeBalanceMode,
                                  float    lfeBalanceGainFactor,
                                  int32_t  lfeFaderMode,
                                  float    lfeFaderGainFactor)
{
   float *pGainLocal = (float *) pG;

   // --- 2. Calculate Fundamental Gain Components ---

   // Balance-related components
   float balanceAngle = (M_PI / 4.0f) * (balance + 1.0f);
   float leftGain     = cosf(balanceAngle);
   float rightGain    = sinf(balanceAngle);

   // Fader-related components
   float faderAngle = (M_PI / 4.0f) * (fade + 1.0f);
   float frontGain  = sinf(faderAngle);
   float backGain   = cosf(faderAngle);
   float sideGain   = 0.0f;

   if (fade < 0.0f) { // Fading to rear
      sideGain = sideGainFactor * backGain + (1 - sideGainFactor) * frontGain;
   }
   else { // Fading to front or center
      sideGain = backGain;
   }

   float topMiddleGain = 0.5f * (frontGain + backGain);
   // --- 3. Loop Through User's Channel Config and Calculate Gains On-The-Fly ---
   for (int32_t i = 0; i < heightIn; i++) {
      int32_t channelType = channelConfig[i];
      // Use a switch to calculate and assign the gain for only this channel
      switch (channelType) {
      // --- Base Layer ---
      case AUDIOLIB_CHANNEL_FRONT_LEFT:
      case AUDIOLIB_CHANNEL_FRONT_LEFT_TREBBLE:
      case AUDIOLIB_CHANNEL_FRONT_LEFT_MID:
      case AUDIOLIB_CHANNEL_FRONT_LEFT_WOOFER:
         pGainLocal[i] = leftGain * frontGain;
         break;
      case AUDIOLIB_CHANNEL_FRONT_RIGHT:
      case AUDIOLIB_CHANNEL_FRONT_RIGHT_TREBBLE:
      case AUDIOLIB_CHANNEL_FRONT_RIGHT_MID:
      case AUDIOLIB_CHANNEL_FRONT_RIGHT_WOOFER:
         pGainLocal[i] = rightGain * frontGain;
         break;
      case AUDIOLIB_CHANNEL_FRONT_CENTER:
         pGainLocal[i] = 1.0f * frontGain;
         break;
      case AUDIOLIB_CHANNEL_SIDE_LEFT:
         pGainLocal[i] = leftGain * sideGain;
         break;
      case AUDIOLIB_CHANNEL_SIDE_RIGHT:
         pGainLocal[i] = rightGain * sideGain;
         break;
      case AUDIOLIB_CHANNEL_REAR_LEFT:
         pGainLocal[i] = leftGain * backGain;
         break;
      case AUDIOLIB_CHANNEL_REAR_RIGHT:
         pGainLocal[i] = rightGain * backGain;
         break;

      // --- Height Layer ---
      case AUDIOLIB_CHANNEL_TOP_FRONT_LEFT:
         pGainLocal[i] = leftGain * frontGain;
         break;
      case AUDIOLIB_CHANNEL_TOP_FRONT_RIGHT:
         pGainLocal[i] = rightGain * frontGain;
         break;
      case AUDIOLIB_CHANNEL_TOP_MIDDLE_LEFT:
         pGainLocal[i] = leftGain * topMiddleGain;
         break;
      case AUDIOLIB_CHANNEL_TOP_MIDDLE_RIGHT:
         pGainLocal[i] = rightGain * topMiddleGain;
         break;
      case AUDIOLIB_CHANNEL_TOP_REAR_LEFT:
         pGainLocal[i] = leftGain * backGain;
         break;
      case AUDIOLIB_CHANNEL_TOP_REAR_RIGHT:
         pGainLocal[i] = rightGain * backGain;
         break;
      case AUDIOLIB_CHANNEL_OVERHEAD:
         pGainLocal[i] = 1.0f * topMiddleGain;
         break;

      // --- LFE (Subwoofer) ---
      case AUDIOLIB_CHANNEL_LOW_FREQUENCY_EFFECTS: {
         float lfeBalanceGain = 1.0f; // Default
         if (lfeBalanceMode == AUDIOLIB_LFE_BALANCE_PARTIAL) {
            lfeBalanceGain = lfeBalanceGainFactor + (1 - lfeBalanceGainFactor) * cosf(2.0f * balanceAngle);
         }
         else if (lfeBalanceMode == AUDIOLIB_LFE_BALANCE_FULL) {
            lfeBalanceGain = 0.5f * (leftGain + rightGain);
         }

         float lfeFaderGain = 1.0f; // Default
         if (lfeFaderMode == AUDIOLIB_LFE_FADER_AVERAGE) {
            lfeFaderGain = 0.5f * (frontGain + backGain);
         }
         else if (lfeFaderMode == AUDIOLIB_LFE_FADER_POSITION_BASED) {
            lfeFaderGain = (1 - lfeFaderGainFactor) * frontGain + lfeFaderGainFactor * backGain;
         }
         else if (lfeFaderMode == AUDIOLIB_LFE_FADER_FULL) {
            lfeFaderGain = backGain;
         }

         pGainLocal[i] = lfeBalanceGain * lfeFaderGain;
      } break;
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_balanceFader_exec_cn(void  *pIn,
                                   void  *pGain,
                                   void  *pOut,
                                   size_t widthIn,
                                   size_t heightIn,
                                   size_t strideIn,
                                   size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal   = (dataType *) pIn;
   dataType *pGainLocal = (dataType *) pGain;
   dataType *pOutLocal  = (dataType *) pOut;
   for (uint32_t i = 0; i < heightIn; i++) {
      dataType gainCh = pGainLocal[i];
      for (uint32_t j = 0; j < widthIn; j++) {
         pOutLocal[i * strideOut + j] = pInLocal[i * strideIn + j] * gainCh;
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_balanceFaderInterLeave_exec_cn(void  *pIn,
                                             void  *pGain,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t strideIn,
                                             size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal   = (dataType *) pIn;
   dataType *pGainLocal = (dataType *) pGain;
   dataType *pOutLocal  = (dataType *) pOut;

   for (uint32_t i = 0; i < widthIn; ++i) {
      dataType gainCh = pGainLocal[i];
      for (uint32_t j = 0; j < heightIn; ++j) {
         pOutLocal[j * strideOut + i] = pInLocal[j * strideIn + i] * gainCh;
      }
   }
   return;
}

template void AUDIOLIB_balanceFader_exec_cn<float>(void  *pIn,
                                                   void  *pGain,
                                                   void  *pOut,
                                                   size_t widthIn,
                                                   size_t heightIn,
                                                   size_t strideIn,
                                                   size_t strideOut);
template void AUDIOLIB_balanceFaderInterLeave_exec_cn<float>(void  *pIn,
                                                             void  *pGain,
                                                             void  *pOut,
                                                             size_t widthIn,
                                                             size_t heightIn,
                                                             size_t strideIn,
                                                             size_t strideOut);
void          AUDIOLIB_balanceFader_cn(void    *pIn,
                                       void    *pGain,
                                       void    *pOut,
                                       uint32_t data_type,
                                       size_t   widthIn,
                                       size_t   heightIn,
                                       size_t   strideIn,
                                       size_t   strideOut,
                                       size_t   isInterleave)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      if (isInterleave == 1) {
         AUDIOLIB_balanceFaderInterLeave_exec_cn<float>(pIn, pGain, pOut, widthIn, heightIn, strideIn, strideOut);
      }
      else {
         AUDIOLIB_balanceFader_exec_cn<float>(pIn, pGain, pOut, widthIn, heightIn, strideIn, strideOut);
      }
   }
}
