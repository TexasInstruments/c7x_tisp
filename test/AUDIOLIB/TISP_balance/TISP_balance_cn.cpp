// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_balance_cn.h"
#include <audiolib.h>
#include <math.h>

#define AUDIOLIB_BALANCE__GAIN_THRESHOLD 1e-6f // Threshold for gain comparison to bypass smoothing

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

extern float currentGainL;
extern float currentGainR;

template <typename dataType>
void AUDIOLIB_balance_exec_cn(void   *pInL,
                              void   *pInR,
                              void   *pOutL,
                              void   *pOutR,
                              size_t  widthIn,
                              size_t  heightIn,
                              size_t  strideInElements,
                              size_t  strideOutElements,
                              float   balance,
                              int32_t samplingRate,
                              float   smoothingTime)
{

   dataType *pInLocalLeft   = (dataType *) pInL;
   dataType *pOutLocalLeft  = (dataType *) pOutL;
   dataType *pInLocalRight  = (dataType *) pInR;
   dataType *pOutLocalRight = (dataType *) pOutR;

   float angle       = (1.0f + balance) * (M_PI / 4.0f);
   float targetGainL = cosf(angle);
   float targetGainR = sinf(angle);

   int32_t bypassSmoothing;
   float   smoothingCoefficient;

   if (smoothingTime != 0.0f) {
      bypassSmoothing = 0;

      smoothingCoefficient = expf(-1.0f / (samplingRate * (smoothingTime / 1000.0f)));
   }
   else {
      bypassSmoothing = 1;
   }

   bypassSmoothing = bypassSmoothing || (fabsf(currentGainL - targetGainL) < (AUDIOLIB_BALANCE__GAIN_THRESHOLD) &&
                                         fabsf(currentGainR - targetGainR) < (AUDIOLIB_BALANCE__GAIN_THRESHOLD));

   if (!bypassSmoothing) {
      // Apply smoothing and compute output on-the-fly
      for (uint32_t j = 0; j < widthIn; j++) {
         // Apply smoothing to the gain for left and right channels
         currentGainL = smoothingCoefficient * currentGainL + (1.0f - smoothingCoefficient) * targetGainL;
         currentGainR = smoothingCoefficient * currentGainR + (1.0f - smoothingCoefficient) * targetGainR;

         // Apply gains to all channels for this sample
         for (uint32_t i = 0; i < heightIn; i++) {
            pOutLocalLeft[j + i * strideOutElements]  = pInLocalLeft[j + i * strideInElements] * currentGainL;
            pOutLocalRight[j + i * strideOutElements] = pInLocalRight[j + i * strideInElements] * currentGainR;
         }
      }

      // Update current gains for the next frame
      currentGainL = currentGainL;
      currentGainR = currentGainR;
   }
   else {
      // Apply the target gain to the input samples and write to output
      for (uint32_t j = 0; j < widthIn; j++) {
         for (uint32_t i = 0; i < heightIn; i++) {
            pOutLocalLeft[j + i * strideOutElements]  = pInLocalLeft[j + i * strideInElements] * targetGainL;
            pOutLocalRight[j + i * strideOutElements] = pInLocalRight[j + i * strideInElements] * targetGainR;
         }
      }
   }
   return;
}

// explicit instantiation for the different data type versions
template void AUDIOLIB_balance_exec_cn<float>(void   *pInL,
                                              void   *pInR,
                                              void   *pOutL,
                                              void   *pOutR,
                                              size_t  widthIn,
                                              size_t  heightIn,
                                              size_t  strideInElements,
                                              size_t  strideOutElements,
                                              float   balance,
                                              int32_t samplingRate,
                                              float   smoothingTime);

void AUDIOLIB_balance_cn(void    *pInL,
                         void    *pInR,
                         void    *pOutL,
                         void    *pOutR,
                         uint32_t data_type,
                         size_t   widthIn,
                         size_t   heightIn,
                         size_t   strideInElements,
                         size_t   strideOutElements,
                         float    balance,
                         int32_t  samplingRate,
                         float    smoothingTime)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_balance_exec_cn<float>(pInL, pInR, pOutL, pOutR, widthIn, heightIn, strideInElements, strideOutElements,
                                      balance, samplingRate, smoothingTime);
   }
}
