// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_mute_cn.h"
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

void AUDIOLIB_mute_cn_set(uint32_t isMute,
                          uint32_t fadeType,
                          float    fadeTime,
                          int32_t  samplingRate,
                          size_t   isInterleaved)
{
   int32_t *bypassSmoothing      = getBypassSmoothing();
   float   *gainStep             = getGainStep();
   float   *alphaMultiplier      = getAlphaMultiplier();
   float   *smoothingCoefficient = getSmoothingCoefficient();
   float   *currentGain          = getCurrentGain();
   float   *targetGain           = getTargetGain();

   *currentGain = isMute ? 1.0f : 0.0f;
   *targetGain  = isMute ? 0.0f : 1.0f;

   float alphaCoeff[ALPHA_COEFF_INDEX];
   float gainStepAdder[ALPHA_COEFF_INDEX];

   switch (fadeType) {
   case 0: {
      float totalFadeSamples = (fadeTime * samplingRate) / 1000.0f;
      *bypassSmoothing       = (int32_t) (fadeTime == 0.0f || totalFadeSamples <= 0);

      if (!*bypassSmoothing) {
         *gainStep = (*targetGain - *currentGain) / totalFadeSamples;
         for (int i = 0; i < ALPHA_COEFF_INDEX; i++) {
            if (isInterleaved) {
               alphaCoeff[i]    = *gainStep;
               gainStepAdder[i] = *gainStep;
            }
            else {
               alphaCoeff[i]    = (i + 1) * *gainStep;
               gainStepAdder[i] = 8 * *gainStep;
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

   case 3: {
      // Geometric ramp between the mute floor (-200 dB) and unity: equal time to mute and unmute
      const float floorGain        = 1e-10f;
      float       totalFadeSamples = (fadeTime * samplingRate) / 1000.0f;
      *bypassSmoothing             = (int32_t) (fadeTime == 0.0f || totalFadeSamples <= 0);

      if (!*bypassSmoothing) {
         *currentGain = isMute ? 1.0f : floorGain;
         *targetGain  = isMute ? floorGain : 1.0f;
         *gainStep    = powf(*targetGain / *currentGain, 1.0f / totalFadeSamples);
      }
      break;
   }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_muteHard_exec_cn(void  *pIn,
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
      for (uint32_t m = 0; m < heightIn; m++) {
         for (uint32_t n = 0; n < widthIn; n++) {
            pOutLocal[m * strideOutElements + n] = pInLocal[m * strideInElements + n] * *targetGain;
         }
      }
   }
   else {
      // Non-interleaved: [ch0_s0, ch0_s1, ..., ch1_s0, ...]
      for (uint32_t n = 0; n < widthIn; n++) {
         for (uint32_t m = 0; m < heightIn; m++) {
            pOutLocal[m * strideOutElements + n] = pInLocal[m * strideInElements + n] * *targetGain;
         }
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_muteLinearFade_exec_cn(void  *pIn,
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

   float localCurrentGain = *currentGain;

   if (isInterleaved) {
      // Interleaved: [ch0_s0, ch1_s0, ..., ch0_s1, ...]
      for (uint32_t j = 0; j < heightIn; j++) {
         localCurrentGain += *gainStep;
         if ((*gainStep > 0 && localCurrentGain > *targetGain) || (*gainStep < 0 && localCurrentGain < *targetGain)) {
            localCurrentGain = *targetGain;
         }

         for (uint32_t c = 0; c < widthIn; c++) {
            pOutLocal[j * strideOutElements + c] = pInLocal[j * strideInElements + c] * localCurrentGain;
         }
      }
   }
   else {
      // Non-interleaved: [ch0_s0, ch0_s1, ..., ch1_s0, ...]
      for (uint32_t j = 0; j < widthIn; j++) {
         localCurrentGain += *gainStep;
         if ((*gainStep > 0 && localCurrentGain > *targetGain) || (*gainStep < 0 && localCurrentGain < *targetGain)) {
            localCurrentGain = *targetGain;
         }

         for (uint32_t c = 0; c < heightIn; c++) {
            pOutLocal[c * strideOutElements + j] = pInLocal[c * strideInElements + j] * localCurrentGain;
         }
      }
   }

   *currentGain = localCurrentGain;
   return;
}

template <typename dataType>
void AUDIOLIB_muteSmoothFade_exec_cn(void  *pIn,
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

   float localCurrentGain = *currentGain;

   if (isInterleaved) {
      // Interleaved: [ch0_s0, ch1_s0, ..., ch0_s1, ...]
      for (uint32_t j = 0; j < heightIn; j++) {
         localCurrentGain = *smoothingCoefficient * localCurrentGain + (1.0f - *smoothingCoefficient) * *targetGain;

         for (uint32_t c = 0; c < widthIn; c++) {
            pOutLocal[j * strideOutElements + c] = pInLocal[j * strideInElements + c] * localCurrentGain;
         }
      }
   }
   else {
      // Non-interleaved: [ch0_s0, ch0_s1, ..., ch1_s0, ...]
      for (uint32_t j = 0; j < widthIn; j++) {
         localCurrentGain = *smoothingCoefficient * localCurrentGain + (1.0f - *smoothingCoefficient) * *targetGain;

         for (uint32_t c = 0; c < heightIn; c++) {
            pOutLocal[c * strideOutElements + j] = pInLocal[c * strideInElements + j] * localCurrentGain;
         }
      }
   }

   *currentGain = localCurrentGain;
   return;
}

template <typename dataType>
void AUDIOLIB_muteSymmetricFade_exec_cn(void  *pIn,
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
   float *gainStep    = getGainStep();

   const float floorGain        = 1e-10f;
   float       localCurrentGain = *currentGain;
   float       appliedGain;

   if (isInterleaved) {
      // Interleaved: [ch0_s0, ch1_s0, ..., ch0_s1, ...]
      for (uint32_t j = 0; j < heightIn; j++) {
         // The ramp moves geometrically, so it is clamped to [floor, 1]; the floor is the fully muted state
         localCurrentGain = fminf(fmaxf(localCurrentGain * *gainStep, floorGain), 1.0f);
         appliedGain      = (localCurrentGain > floorGain) ? localCurrentGain : 0.0f;

         for (uint32_t c = 0; c < widthIn; c++) {
            pOutLocal[j * strideOutElements + c] = pInLocal[j * strideInElements + c] * appliedGain;
         }
      }
   }
   else {
      // Non-interleaved: [ch0_s0, ch0_s1, ..., ch1_s0, ...]
      for (uint32_t j = 0; j < widthIn; j++) {
         localCurrentGain = fminf(fmaxf(localCurrentGain * *gainStep, floorGain), 1.0f);
         appliedGain      = (localCurrentGain > floorGain) ? localCurrentGain : 0.0f;

         for (uint32_t c = 0; c < heightIn; c++) {
            pOutLocal[c * strideOutElements + j] = pInLocal[c * strideInElements + j] * appliedGain;
         }
      }
   }

   *currentGain = localCurrentGain;
   return;
}

template void AUDIOLIB_muteHard_exec_cn<float>(void  *pIn,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideInElements,
                                               size_t strideOutElements,
                                               size_t isInterleaved);

template void AUDIOLIB_muteLinearFade_exec_cn<float>(void  *pIn,
                                                     void  *pOut,
                                                     size_t widthIn,
                                                     size_t heightIn,
                                                     size_t strideInElements,
                                                     size_t strideOutElements,
                                                     size_t isInterleaved);

template void AUDIOLIB_muteSmoothFade_exec_cn<float>(void  *pIn,
                                                     void  *pOut,
                                                     size_t widthIn,
                                                     size_t heightIn,
                                                     size_t strideInElements,
                                                     size_t strideOutElements,
                                                     size_t isInterleaved);

template void AUDIOLIB_muteSymmetricFade_exec_cn<float>(void  *pIn,
                                                        void  *pOut,
                                                        size_t widthIn,
                                                        size_t heightIn,
                                                        size_t strideInElements,
                                                        size_t strideOutElements,
                                                        size_t isInterleaved);

void AUDIOLIB_mute_cn(void    *pIn,
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
      AUDIOLIB_muteHard_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements,
                                       isInterleaved);
   }
   else {
      // Otherwise, select the appropriate natural C fade function.
      switch (fadeType) {
      case 0:
         AUDIOLIB_muteLinearFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements,
                                                isInterleaved);
         break;

      case 1:
         AUDIOLIB_muteSmoothFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements,
                                                isInterleaved);
         break;

      case 3:
         AUDIOLIB_muteSymmetricFade_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements,
                                                   isInterleaved);
         break;
      }
   }
}
