// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_rmsSmoothed_cn.h"
#include <audiolib.h>
#include <float.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

float *getSmoothingCoefficient();
float *getSmoothedEnergy();

void AUDIOLIB_rmsSmoothed_cn_set(int32_t samplingRate, float smoothingTime, size_t widthIn)
{
   float *smoothingCoefficient = getSmoothingCoefficient();
   float *smoothedEnergy       = getSmoothedEnergy();

   // alpha = exp(-2 pi B / (tau fs)), tau = smoothingTime / 1000; a smoothing time of 0 disables the smoothing
   if (smoothingTime == 0.0f) {
      *smoothingCoefficient = 0.0f;
   }
   else {
      *smoothingCoefficient = expf(-2.0f * M_PI * (float) widthIn / ((float) samplingRate * (smoothingTime / 1000.0f)));
   }
   *smoothedEnergy = 0.0f;
   return;
}

template <typename dataType>
void AUDIOLIB_rmsSmoothed_exec_cn(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal             = (dataType *) pIn;
   dataType *pOutLocal            = (dataType *) pOut;
   dataType  smoothingCoefficient = (dataType) *getSmoothingCoefficient();
   float    *smoothedEnergy       = getSmoothedEnergy();

   // Mean energy of the block over all channels: rows are channels, columns are samples
   dataType energy = 0;
   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         energy += pInLocal[i * strideIn + j] * pInLocal[i * strideIn + j];
      }
   }
   energy = energy * (dataType) (1.0f / (float) (widthIn * heightIn));

   // y[k] = alpha * y[k-1] + (1 - alpha) * E[k]; the smoothed energy is carried as float
   dataType y      = (smoothingCoefficient * (dataType) *smoothedEnergy) + ((1 - smoothingCoefficient) * energy);
   *smoothedEnergy = (float) y;

   pOutLocal[0] = (dataType) sqrt(y);
   return;
}

template void
AUDIOLIB_rmsSmoothed_exec_cn<float>(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn);
template void
AUDIOLIB_rmsSmoothed_exec_cn<double>(void *pIn, void *pOut, size_t widthIn, size_t heightIn, size_t strideIn);

void AUDIOLIB_rmsSmoothed_cn(void    *pIn,
                             void    *pOut,
                             uint32_t data_type,
                             size_t   widthIn,
                             size_t   heightIn,
                             size_t   strideIn)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_rmsSmoothed_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_rmsSmoothed_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn);
   }
}
