// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_gainNChTrimSmoothing_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

#define GAINNCHTRIMSMOOTHING_CN_GAIN_FLOOR (1.0e-10f)

float    *getCurrentGain();
float    *getTargetGain();
float    *getGainStep();
int32_t  *getRemainingFadeSamples();
float    *getSmoothingCoefficient();
uint32_t *getFadeType();

/* Loads new gains, as one set() call does. A non-null pInitialGain restarts every channel at rest on that gain.
 * The target of channel ch is masterGain * pTrimGain[ch]. Symmetric (fadeType 0): a constant ratio per sample
 * that reaches the target after fadeTime ms. Smooth (fadeType 1): a one-pole smoother with time constant fadeTime ms.
 */
void AUDIOLIB_gainNChTrimSmoothing_cn_set(float   *pInitialGain,
                                          float   *pTrimGain,
                                          float    masterGain,
                                          float    fadeTime,
                                          uint32_t fadeType,
                                          float    samplingRate,
                                          size_t   numChannels)
{
   float    *currentGain          = getCurrentGain();
   float    *targetGain           = getTargetGain();
   float    *gainStep             = getGainStep();
   int32_t  *remainingFadeSamples = getRemainingFadeSamples();
   float    *smoothingCoefficient = getSmoothingCoefficient();
   uint32_t *currentFadeType      = getFadeType();

   int32_t totalFadeSamples = (int32_t) ((fadeTime / 1000.0f) * samplingRate);
   *currentFadeType         = fadeType;
   *smoothingCoefficient    = (fadeTime > 0.0f) ? (float) (1.0 - exp(-1.0 / (samplingRate * fadeTime / 1000.0))) : 1.0f;

   for (size_t ch = 0; ch < numChannels; ch++) {
      if (pInitialGain != NULL) {
         currentGain[ch]          = pInitialGain[ch];
         targetGain[ch]           = pInitialGain[ch];
         gainStep[ch]             = 1.0f;
         remainingFadeSamples[ch] = 0;
      }
      float target = masterGain * pTrimGain[ch];
      if (fadeType == 1U) {
         targetGain[ch] = target;
      }
      else {
         int32_t changed = (fabsf(target - targetGain[ch]) > (1.0e-6f * fabsf(targetGain[ch]) + 1.0e-8f)) ? 1 : 0;
         if ((changed == 0) && (remainingFadeSamples[ch] > 0)) {
            /* Same target while fading: keep the fade */
         }
         else if ((changed == 1) && (totalFadeSamples > 0)) {
            float start = (currentGain[ch] > GAINNCHTRIMSMOOTHING_CN_GAIN_FLOOR) ? currentGain[ch]
                                                                                 : GAINNCHTRIMSMOOTHING_CN_GAIN_FLOOR;
            float end   = (target > GAINNCHTRIMSMOOTHING_CN_GAIN_FLOOR) ? target : GAINNCHTRIMSMOOTHING_CN_GAIN_FLOOR;
            currentGain[ch]          = start;
            gainStep[ch]             = powf(end / start, 1.0f / (float) totalFadeSamples);
            remainingFadeSamples[ch] = totalFadeSamples;
            targetGain[ch]           = target;
         }
         else {
            currentGain[ch]          = target;
            gainStep[ch]             = 1.0f;
            remainingFadeSamples[ch] = 0;
            targetGain[ch]           = target;
         }
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_gainNChTrimSmoothingSymmetricFade_exec_cn(void  *pIn,
                                                        void  *pOut,
                                                        size_t widthIn,
                                                        size_t heightIn,
                                                        size_t strideIn,
                                                        size_t strideOut,
                                                        size_t isInterleave)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   float   *currentGain          = getCurrentGain();
   float   *targetGain           = getTargetGain();
   float   *gainStep             = getGainStep();
   int32_t *remainingFadeSamples = getRemainingFadeSamples();

   if (isInterleave == 1) {
      // Interleaved: rows are samples, columns are channels
      for (uint32_t i = 0; i < widthIn; i++) {
         for (uint32_t j = 0; j < heightIn; j++) {
            dataType gain;
            if (remainingFadeSamples[i] > 0) {
               gain = currentGain[i];
               currentGain[i] *= gainStep[i];
               remainingFadeSamples[i]--;
               if (remainingFadeSamples[i] == 0) {
                  currentGain[i] = targetGain[i];
               }
            }
            else {
               gain           = targetGain[i];
               currentGain[i] = targetGain[i];
            }
            pOutLocal[j * strideOut + i] = pInLocal[j * strideIn + i] * gain;
         }
      }
   }
   else {
      // Non-interleaved: rows are channels, columns are samples
      for (uint32_t i = 0; i < heightIn; i++) {
         for (uint32_t j = 0; j < widthIn; j++) {
            dataType gain;
            if (remainingFadeSamples[i] > 0) {
               gain = currentGain[i];
               currentGain[i] *= gainStep[i];
               remainingFadeSamples[i]--;
               if (remainingFadeSamples[i] == 0) {
                  currentGain[i] = targetGain[i];
               }
            }
            else {
               gain           = targetGain[i];
               currentGain[i] = targetGain[i];
            }
            pOutLocal[i * strideOut + j] = pInLocal[i * strideIn + j] * gain;
         }
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_gainNChTrimSmoothingSmoothFade_exec_cn(void  *pIn,
                                                     void  *pOut,
                                                     size_t widthIn,
                                                     size_t heightIn,
                                                     size_t strideIn,
                                                     size_t strideOut,
                                                     size_t isInterleave)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   float *currentGain          = getCurrentGain();
   float *targetGain           = getTargetGain();
   float  smoothingCoefficient = *getSmoothingCoefficient();

   if (isInterleave == 1) {
      // Interleaved: rows are samples, columns are channels
      for (uint32_t i = 0; i < widthIn; i++) {
         for (uint32_t j = 0; j < heightIn; j++) {
            currentGain[i] = currentGain[i] * (1.0f - smoothingCoefficient) + targetGain[i] * smoothingCoefficient;
            dataType gain  = currentGain[i];
            pOutLocal[j * strideOut + i] = pInLocal[j * strideIn + i] * gain;
         }
      }
   }
   else {
      // Non-interleaved: rows are channels, columns are samples
      for (uint32_t i = 0; i < heightIn; i++) {
         for (uint32_t j = 0; j < widthIn; j++) {
            currentGain[i] = currentGain[i] * (1.0f - smoothingCoefficient) + targetGain[i] * smoothingCoefficient;
            dataType gain  = currentGain[i];
            pOutLocal[i * strideOut + j] = pInLocal[i * strideIn + j] * gain;
         }
      }
   }
   return;
}

template void AUDIOLIB_gainNChTrimSmoothingSymmetricFade_exec_cn<float>(void  *pIn,
                                                                        void  *pOut,
                                                                        size_t widthIn,
                                                                        size_t heightIn,
                                                                        size_t strideIn,
                                                                        size_t strideOut,
                                                                        size_t isInterleave);
template void AUDIOLIB_gainNChTrimSmoothingSmoothFade_exec_cn<float>(void  *pIn,
                                                                     void  *pOut,
                                                                     size_t widthIn,
                                                                     size_t heightIn,
                                                                     size_t strideIn,
                                                                     size_t strideOut,
                                                                     size_t isInterleave);

void AUDIOLIB_gainNChTrimSmoothing_cn(void    *pIn,
                                      void    *pOut,
                                      uint32_t data_type,
                                      size_t   widthIn,
                                      size_t   heightIn,
                                      size_t   strideIn,
                                      size_t   strideOut,
                                      size_t   isInterleave)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      switch (*getFadeType()) {
      case 0:
         AUDIOLIB_gainNChTrimSmoothingSymmetricFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut,
                                                                   isInterleave);
         break;

      case 1:
         AUDIOLIB_gainNChTrimSmoothingSmoothFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut,
                                                                isInterleave);
         break;
      }
   }
}
