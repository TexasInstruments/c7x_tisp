// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_gainSmoothing_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

#define GAINSMOOTHING_CN_GAIN_EPSILON (1.0e-6f)

float    *getCurrentGain();
float    *getTargetGain();
float    *getGainStep();
float    *getSmoothingCoefficient();
uint32_t *getFadeType();

/* Starts a ramp from currentGain to targetGain, as one set() call does. Symmetric (fadeType 0): a constant ratio per
 * sample that reaches the target after fadeTime ms. Smooth (fadeType 1): a one-pole smoother with time constant
 * fadeTime ms. fadeTime 0 applies the target at once.
 */
void AUDIOLIB_gainSmoothing_cn_set(float    currentGain,
                                   float    targetGain,
                                   float    fadeTime,
                                   uint32_t fadeType,
                                   float    samplingRate)
{
   *getCurrentGain()          = (fadeTime > 0.0f) ? currentGain : targetGain;
   *getTargetGain()           = targetGain;
   *getFadeType()             = fadeType;
   *getGainStep()             = 1.0f;
   *getSmoothingCoefficient() = 0.0f;

   if ((fadeTime > 0.0f) && (fadeType == 0U)) {
      float totalFadeSamples = (fadeTime * samplingRate) / 1000.0f;
      *getGainStep()         = powf(targetGain / currentGain, 1.0f / totalFadeSamples);
   }
   else if (fadeTime > 0.0f) {
      *getSmoothingCoefficient() = expf(-1.0f / (samplingRate * (fadeTime / 1000.0f)));
   }
   return;
}

template <typename dataType>
void AUDIOLIB_gainSmoothingSymmetricFade_exec_cn(void  *pIn,
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

   float *currentGain = getCurrentGain();
   float  targetGain  = *getTargetGain();
   float  gainStep    = *getGainStep();
   int    isFadingUp  = (targetGain > *currentGain) ? 1 : 0;

   // One gain per sample: samples are rows when interleaved, columns otherwise
   size_t numSamples  = (isInterleave == 1) ? heightIn : widthIn;
   size_t numChannels = (isInterleave == 1) ? widthIn : heightIn;

   for (uint32_t s = 0; s < numSamples; s++) {
      float gain = targetGain;
      if (fabsf(*currentGain - targetGain) >= GAINSMOOTHING_CN_GAIN_EPSILON) {
         gain = *currentGain * gainStep;
         // The ramp overshoots by design; clamp it at the target
         if (((isFadingUp == 1) && (gain > targetGain)) || ((isFadingUp == 0) && (gain < targetGain))) {
            gain = targetGain;
         }
      }
      *currentGain = gain;
      for (uint32_t ch = 0; ch < numChannels; ch++) {
         size_t inIdx      = (isInterleave == 1) ? (s * strideIn + ch) : (ch * strideIn + s);
         size_t outIdx     = (isInterleave == 1) ? (s * strideOut + ch) : (ch * strideOut + s);
         pOutLocal[outIdx] = pInLocal[inIdx] * gain;
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_gainSmoothingSmoothFade_exec_cn(void  *pIn,
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
   float  targetGain           = *getTargetGain();
   float  smoothingCoefficient = *getSmoothingCoefficient();

   // One gain per sample: samples are rows when interleaved, columns otherwise
   size_t numSamples  = (isInterleave == 1) ? heightIn : widthIn;
   size_t numChannels = (isInterleave == 1) ? widthIn : heightIn;

   for (uint32_t s = 0; s < numSamples; s++) {
      *currentGain = (smoothingCoefficient * *currentGain) + ((1.0f - smoothingCoefficient) * targetGain);
      for (uint32_t ch = 0; ch < numChannels; ch++) {
         size_t inIdx      = (isInterleave == 1) ? (s * strideIn + ch) : (ch * strideIn + s);
         size_t outIdx     = (isInterleave == 1) ? (s * strideOut + ch) : (ch * strideOut + s);
         pOutLocal[outIdx] = pInLocal[inIdx] * (*currentGain);
      }
   }
   return;
}

template void AUDIOLIB_gainSmoothingSymmetricFade_exec_cn<float>(void  *pIn,
                                                                 void  *pOut,
                                                                 size_t widthIn,
                                                                 size_t heightIn,
                                                                 size_t strideIn,
                                                                 size_t strideOut,
                                                                 size_t isInterleave);
template void AUDIOLIB_gainSmoothingSmoothFade_exec_cn<float>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t strideIn,
                                                              size_t strideOut,
                                                              size_t isInterleave);

void AUDIOLIB_gainSmoothing_cn(void    *pIn,
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
         AUDIOLIB_gainSmoothingSymmetricFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut,
                                                            isInterleave);
         break;

      case 1:
         AUDIOLIB_gainSmoothingSmoothFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut,
                                                         isInterleave);
         break;
      }
   }
}
