// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_sinusoidGenerator_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

extern float phase, phaseIncTarget, phaseInc, smoothingCoefficient, oneMinusSmoothingCoefficient;

void AUDIOLIB_sinusoidGenerator_cn_set(float   startPhase,
                                       float   startFrequency,
                                       float   targetFrequency,
                                       int32_t samplingRate,
                                       float   smoothingTime)
{
   float PI = 3.14159265358979323846;
   phase    = startPhase * PI / 180.0;
   if (smoothingTime != 0.0f) {
      phaseIncTarget               = 2 * PI * targetFrequency / samplingRate;
      phaseInc                     = 2 * PI * startFrequency / samplingRate;
      smoothingCoefficient         = 1 - expf(-1 / (samplingRate * smoothingTime / 1000));
      oneMinusSmoothingCoefficient = 1 - smoothingCoefficient;
   }
   else {
      smoothingCoefficient         = 1.0f;
      oneMinusSmoothingCoefficient = 0.0f;
      phaseIncTarget               = 2 * PI * targetFrequency / samplingRate;
      startFrequency               = targetFrequency;
      phaseInc                     = phaseIncTarget;
   }
}
template <typename dataType> void AUDIOLIB_sinusoidGenerator_exec_cn(void *pOut, size_t widthIn)
{
   /* Typecast void pointers to respective data type */
   dataType *RESTRICT pOutLocal = (dataType *) pOut;

   float phaseCn, phaseIncCn;

   dataType TWO_PIF = 2 * 3.14159265358979323846;
   dataType MAX_VAL = 1048576.0f;

   phaseCn    = phase;
   phaseIncCn = phaseInc;

   dataType phases[widthIn];

   for (uint32_t i = 0; i < widthIn; i++) {
      phases[i] = phaseCn;

      if (fabs(phases[i]) >= MAX_VAL) {
         pOutLocal[i] = 0.0f;
      }
      else {
         pOutLocal[i] = sinf(phases[i]);
      }
      phaseCn += phaseIncCn;

      if (phaseCn >= TWO_PIF) {
         phaseCn -= TWO_PIF;
      }
      phaseIncCn = phaseIncCn * oneMinusSmoothingCoefficient + phaseIncTarget * smoothingCoefficient;
   }
   phase    = phaseCn;
   phaseInc = phaseIncCn;

   return;
}

template void AUDIOLIB_sinusoidGenerator_exec_cn<float>(void *pOut, size_t widthIn);

void AUDIOLIB_sinusoidGenerator_cn(void *pOut, uint32_t data_type, size_t widthIn)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_sinusoidGenerator_exec_cn<float>(pOut, widthIn);
   }
}
