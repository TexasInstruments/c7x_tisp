// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_agc_cn.h"
#include <audiolib.h>
#include <float.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

float *getEnergy();
float *getTargetGain();
float *getCurrentGain();
float *getOneOverSlope();
float *getSmoothingCoeffSample();
float *getSmoothingCoeffBlock();
float *getRecoveryRateUp();
float *getRecoveryRateDown();
float *getOneOverNumSamples();

/* Initial state and derived coefficients, as AUDIOLIB_agc_init and AUDIOLIB_agc_set compute them */
void AUDIOLIB_agc_cn_set(int32_t samplingRate,
                         float   currentGain,
                         float   ratio,
                         float   smoothingTime,
                         float   recoveryRate,
                         size_t  numSamples,
                         size_t  numChannels)
{
   float blockTime        = (float) numSamples / (float) samplingRate;
   float smoothingTimeSec = smoothingTime / 1000.0f;
   float recoveryStepDB   = recoveryRate * blockTime;

   *getEnergy()               = 0.0f;
   *getTargetGain()           = 1.0f;
   *getCurrentGain()          = currentGain;
   *getOneOverSlope()         = 1.0f - (1.0f / ratio);
   *getSmoothingCoeffSample() = 1.0f - expf(-1.0f / ((float) samplingRate * smoothingTimeSec));
   *getSmoothingCoeffBlock()  = 1.0f - expf(-blockTime / smoothingTimeSec);
   *getRecoveryRateUp()       = powf(10.0f, recoveryStepDB / 20.0f);
   *getRecoveryRateDown()     = powf(10.0f, -recoveryStepDB / 20.0f);
   *getOneOverNumSamples()    = 1.0f / ((float) numSamples * (float) numChannels);
   return;
}

template <typename dataType>
void AUDIOLIB_agc_exec_cn(void    *pIn,
                          void    *pOut,
                          size_t   widthIn,
                          size_t   heightIn,
                          size_t   strideIn,
                          size_t   strideOut,
                          float    targetLevel,
                          float    maxAttenuation,
                          float    maxGain,
                          float    activationThreshold,
                          uint32_t enableRecovery)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;
   float    *energy    = getEnergy();
   float    *target    = getTargetGain();
   float    *current   = getCurrentGain();

   // Mean energy of the block over all channels and samples, smoothed block by block
   dataType energySum = 0;
   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType x = pInLocal[i * strideIn + j];
         energySum += x * x;
      }
   }
   float blockEnergy = (float) energySum * *getOneOverNumSamples();
   *energy           = (*energy * (1.0f - *getSmoothingCoeffBlock())) + (blockEnergy * *getSmoothingCoeffBlock());

   // Target gain: follows the level above the activation threshold, else held or recovered to 0 dB
   float level = (*energy > 0.0f) ? (10.0f * log10f(*energy)) : -FLT_MAX;
   if (level >= activationThreshold) {
      float gainDB = (targetLevel - level) * *getOneOverSlope();
      gainDB       = (gainDB > maxGain) ? maxGain : gainDB;
      gainDB       = (gainDB < -maxAttenuation) ? -maxAttenuation : gainDB;
      *target      = powf(10.0f, gainDB / 20.0f);
   }
   else if (enableRecovery != 0U) {
      if (*target > 1.0f) {
         *target = *target * *getRecoveryRateDown();
         *target = (*target < 1.0f) ? 1.0f : *target;
      }
      else if (*target < 1.0f) {
         *target = *target * *getRecoveryRateUp();
         *target = (*target > 1.0f) ? 1.0f : *target;
      }
   }

   // One gain chain across all channels and samples, smoothed sample by sample towards the target
   dataType targetGain  = (dataType) *target;
   dataType decay       = (dataType) (1.0f - *getSmoothingCoeffSample());
   dataType currentGain = (dataType) *current;
   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         currentGain                  = targetGain + (decay * (currentGain - targetGain));
         pOutLocal[i * strideOut + j] = pInLocal[i * strideIn + j] * currentGain;
      }
   }
   *current = (float) currentGain;
   return;
}

template void AUDIOLIB_agc_exec_cn<float>(void    *pIn,
                                          void    *pOut,
                                          size_t   widthIn,
                                          size_t   heightIn,
                                          size_t   strideIn,
                                          size_t   strideOut,
                                          float    targetLevel,
                                          float    maxAttenuation,
                                          float    maxGain,
                                          float    activationThreshold,
                                          uint32_t enableRecovery);
template void AUDIOLIB_agc_exec_cn<double>(void    *pIn,
                                           void    *pOut,
                                           size_t   widthIn,
                                           size_t   heightIn,
                                           size_t   strideIn,
                                           size_t   strideOut,
                                           float    targetLevel,
                                           float    maxAttenuation,
                                           float    maxGain,
                                           float    activationThreshold,
                                           uint32_t enableRecovery);

void AUDIOLIB_agc_cn(void    *pIn,
                     void    *pOut,
                     uint32_t data_type,
                     size_t   widthIn,
                     size_t   heightIn,
                     size_t   strideIn,
                     size_t   strideOut,
                     float    targetLevel,
                     float    maxAttenuation,
                     float    maxGain,
                     float    activationThreshold,
                     uint32_t enableRecovery)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_agc_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut, targetLevel, maxAttenuation,
                                  maxGain, activationThreshold, enableRecovery);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_agc_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn, strideOut, targetLevel, maxAttenuation,
                                   maxGain, activationThreshold, enableRecovery);
   }
}
