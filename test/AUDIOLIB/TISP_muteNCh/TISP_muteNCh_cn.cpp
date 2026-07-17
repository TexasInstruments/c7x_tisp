// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_muteNCh_cn.h"
#include <audiolib.h>
#include <math.h>

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

int32_t *getBypassSmoothing();
float   *getGainStep();
float   *getAlphaMultiplier();
float   *getSmoothingCoefficient();
float   *getCurrentGain();
float   *getTargetGain();

void AUDIOLIB_muteNCh_cn_set(uint32_t *isMute,
                             uint32_t  fadeType,
                             float     fadeTime,
                             int32_t   samplingRate,
                             size_t    isInterleaved,
                             size_t    inChannels)
{
   int32_t *bypassSmoothing      = getBypassSmoothing();
   float   *gainStep             = getGainStep();
   float   *alphaMultiplier      = getAlphaMultiplier();
   float   *smoothingCoefficient = getSmoothingCoefficient();
   float   *currentGain          = getCurrentGain();
   float   *targetGain           = getTargetGain();

   for (uint32_t ch = 0; ch < inChannels; ch++) {
      int32_t mute    = isMute[ch];
      currentGain[ch] = (mute == 0) ? 0.0f : 1.0f;
      targetGain[ch]  = (mute == 0) ? 1.0f : 0.0f;
   }

   float alphaCoeff[ALPHA_COEFF_INDEX];
   float gainStepAdder[ALPHA_COEFF_INDEX];

   switch (fadeType) {
   case 0: {
      float totalFadeSamples         = (fadeTime * samplingRate) / 1000.0f;
      float reciprocalMaxFadeSamples = 1 / totalFadeSamples;
      *bypassSmoothing               = (int32_t) (fadeTime == 0.0f || totalFadeSamples <= 0);

      if (!*bypassSmoothing) {
         for (uint32_t ch = 0; ch < inChannels; ch++) {
            gainStep[ch] = (targetGain[ch] - currentGain[ch]) * reciprocalMaxFadeSamples;
         }
         for (int i = 0; i < ALPHA_COEFF_INDEX; i++) {
            if (isInterleaved) {
               alphaCoeff[i]    = (i + 1) * gainStep[0];
               gainStepAdder[i] = 8 * gainStep[0];
            }
         }
         *alphaMultiplier = 0.0f;
      }
      break;
   }

   case 1: {
      *bypassSmoothing = (int32_t) (fadeTime == 0.0f);
      if (!*bypassSmoothing) {
         float timeConstant    = fadeTime / 1000.0f;
         *smoothingCoefficient = expf(-1.0f / (samplingRate * timeConstant));
         for (int i = 0; i < ALPHA_COEFF_INDEX; i++) {
            if (isInterleaved) {
               alphaCoeff[i] = *smoothingCoefficient;
            }
            else {
               alphaCoeff[i] = (i == 0) ? *smoothingCoefficient : alphaCoeff[i - 1] * *smoothingCoefficient;
            }
         }
         *alphaMultiplier = alphaCoeff[ALPHA_COEFF_INDEX - 1];
      }
      break;
   }

   case 2: {
      *bypassSmoothing = (int32_t) 1;
      break;
   }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_muteNChHard_exec_cn(void  *pIn,
                                  void  *pOut,
                                  size_t widthIn,
                                  size_t heightIn,
                                  size_t strideInElements,
                                  size_t strideOutElements,
                                  size_t isInterleaved)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocal  = (dataType *) pIn;
   dataType *restrict pOutLocal = (dataType *) pOut;

   float *targetGain = getTargetGain();

   if (isInterleaved) {
      // Interleaved: [ch0_s0, ch1_s0, ..., ch0_s1, ...]
      for (uint32_t m = 0; m < widthIn; m++) {
         for (uint32_t n = 0; n < heightIn; n++) {
            pOutLocal[m * strideOutElements + n] = pInLocal[m * strideInElements + n] * targetGain[n];
         }
      }
   }
   else {
      // Non-interleaved: [ch0_s0, ch0_s1, ..., ch1_s0, ...]
      for (uint32_t n = 0; n < widthIn; n++) {
         for (uint32_t m = 0; m < heightIn; m++) {
            pOutLocal[m * strideOutElements + n] = pInLocal[m * strideInElements + n] * targetGain[m];
         }
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_muteNChLinearFade_exec_cn(void  *pIn,
                                        void  *pOut,
                                        size_t widthIn,
                                        size_t heightIn,
                                        size_t strideInElements,
                                        size_t strideOutElements,
                                        size_t isInterleaved)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocal  = (dataType *) pIn;
   dataType *restrict pOutLocal = (dataType *) pOut;

   float *currentGain = getCurrentGain();
   float *targetGain  = getTargetGain();
   float *gainStep    = getGainStep();

   float localCurrentGain[heightIn];
   for (uint32_t ch = 0; ch < heightIn; ch++) {
      localCurrentGain[ch] = currentGain[ch];
   }

   if (isInterleaved) {
      // Interleaved: [ch0_s0, ch1_s0, ..., ch0_s1, ...]
      for (uint32_t j = 0; j < widthIn; j++) {
         for (uint32_t c = 0; c < heightIn; c++) {
            localCurrentGain[c] += gainStep[c];
            if ((gainStep[c] > 0 && localCurrentGain[c] > targetGain[c]) ||
                (gainStep[c] < 0 && localCurrentGain[c] < targetGain[c])) {
               localCurrentGain[c] = targetGain[c];
            }
         }

         for (uint32_t c = 0; c < heightIn; c++) {
            pOutLocal[j * strideOutElements + c] = pInLocal[j * strideInElements + c] * localCurrentGain[c];
         }
      }
   }
   else {
      // Non-interleaved: [ch0_s0, ch0_s1, ..., ch1_s0, ...]
      for (uint32_t j = 0; j < widthIn; j++) {
         for (uint32_t c = 0; c < heightIn; c++) {
            localCurrentGain[c] += gainStep[c];
            if ((gainStep[c] > 0 && localCurrentGain[c] > targetGain[c]) ||
                (gainStep[c] < 0 && localCurrentGain[c] < targetGain[c])) {
               localCurrentGain[c] = targetGain[c];
            }
            pOutLocal[c * strideOutElements + j] = pInLocal[c * strideInElements + j] * localCurrentGain[c];
         }
      }
   }

   for (uint32_t ch = 0; ch < heightIn; ch++) {
      currentGain[ch] = localCurrentGain[ch];
   }
   return;
}

template <typename dataType>
void AUDIOLIB_muteNChSmoothFade_exec_cn(void  *pIn,
                                        void  *pOut,
                                        size_t widthIn,
                                        size_t heightIn,
                                        size_t strideInElements,
                                        size_t strideOutElements,
                                        size_t isInterleaved)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocal  = (dataType *) pIn;
   dataType *restrict pOutLocal = (dataType *) pOut;

   float *currentGain          = getCurrentGain();
   float *targetGain           = getTargetGain();
   float *smoothingCoefficient = getSmoothingCoefficient();

   float localCurrentGain[heightIn]; // Local copy per channel for this frame
   for (uint32_t ch = 0; ch < heightIn; ch++) {
      localCurrentGain[ch] = currentGain[ch];
   }

   if (isInterleaved) {
      // Interleaved: [ch0_s0, ch1_s0, ..., ch0_s1, ...]
      for (uint32_t j = 0; j < widthIn; j++) {
         for (uint32_t c = 0; c < heightIn; c++) {
            localCurrentGain[c] =
                *smoothingCoefficient * localCurrentGain[c] + (1.0f - *smoothingCoefficient) * targetGain[c];
         }
         for (uint32_t c = 0; c < heightIn; c++) {
            pOutLocal[j * strideOutElements + c] = pInLocal[j * strideInElements + c] * localCurrentGain[c];
         }
      }
   }
   else {
      // Non-interleaved: [ch0_s0, ch0_s1, ..., ch1_s0, ...]
      for (uint32_t j = 0; j < widthIn; j++) {

         for (uint32_t c = 0; c < heightIn; c++) {
            localCurrentGain[c] =
                *smoothingCoefficient * localCurrentGain[c] + (1.0f - *smoothingCoefficient) * targetGain[c];
            pOutLocal[c * strideOutElements + j] = pInLocal[c * strideInElements + j] * localCurrentGain[c];
         }
      }
   }

   // Save updated currentGains back
   for (uint32_t ch = 0; ch < heightIn; ch++) {
      currentGain[ch] = localCurrentGain[ch];
   }

   return;
}

template void AUDIOLIB_muteNChHard_exec_cn<float>(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t strideInElements,
                                                  size_t strideOutElements,
                                                  size_t isInterleaved);

template void AUDIOLIB_muteNChLinearFade_exec_cn<float>(void  *pIn,
                                                        void  *pOut,
                                                        size_t widthIn,
                                                        size_t heightIn,
                                                        size_t strideInElements,
                                                        size_t strideOutElements,
                                                        size_t isInterleaved);

template void AUDIOLIB_muteNChSmoothFade_exec_cn<float>(void  *pIn,
                                                        void  *pOut,
                                                        size_t widthIn,
                                                        size_t heightIn,
                                                        size_t strideInElements,
                                                        size_t strideOutElements,
                                                        size_t isInterleaved);

void AUDIOLIB_muteNCh_cn(void    *pIn,
                         void    *pOut,
                         size_t   widthIn,
                         size_t   heightIn,
                         size_t   strideInElements,
                         size_t   strideOutElements,
                         size_t   isInterleaved,
                         int32_t  bypassSmoothing,
                         uint32_t fadeType)
{
   // If smoothing is bypassed, always use the 'Hard' function.
   if (bypassSmoothing) {
      AUDIOLIB_muteNChHard_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements,
                                          isInterleaved);
   }
   else {
      // Otherwise, select the appropriate natural C fade function.
      switch (fadeType) {
      case 0:
         AUDIOLIB_muteNChLinearFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements,
                                                   isInterleaved);
         break;

      case 1:
         AUDIOLIB_muteNChSmoothFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements,
                                                   isInterleaved);
         break;
      }
   }
}
