// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_envelopeDetector_cn.h"
#include <audiolib.h>
#include <float.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

float *getAttackCoefficient();
float *getReleaseCoefficient();
float *getEnvelopeStates();

void AUDIOLIB_envelopeDetector_cn_set(int32_t samplingRate, float attackTime, float releaseTime, size_t numChannels)
{
   float *attackCoefficient  = getAttackCoefficient();
   float *releaseCoefficient = getReleaseCoefficient();
   float *envelopeStates     = getEnvelopeStates();

   *attackCoefficient  = 1.0f - expf(-1.0f / ((float) samplingRate * attackTime));
   *releaseCoefficient = 1.0f - expf(-1.0f / ((float) samplingRate * releaseTime));
   for (size_t ch = 0; ch < numChannels; ch++) {
      envelopeStates[ch] = 0.0f;
   }
   return;
}

template <typename dataType>
void AUDIOLIB_envelopeDetector_exec_cn(void  *pIn,
                                       void  *pOut,
                                       size_t widthIn,
                                       size_t heightIn,
                                       size_t strideIn,
                                       size_t strideOut)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal           = (dataType *) pIn;
   dataType *pOutLocal          = (dataType *) pOut;
   float     attackCoefficient  = *getAttackCoefficient();
   float     releaseCoefficient = *getReleaseCoefficient();
   float    *envelopeStates     = getEnvelopeStates();

   // Non-interleaved: rows are channels, columns are samples
   for (uint32_t i = 0; i < heightIn; i++) {
      dataType envelope = (dataType) envelopeStates[i];
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType x                   = pInLocal[i * strideIn + j];
         dataType magnitude           = (x >= 0) ? x : -x;
         float    coefficient         = (magnitude > envelope) ? attackCoefficient : releaseCoefficient;
         envelope                     = (dataType) ((1.0f - coefficient) * envelope + coefficient * magnitude);
         pOutLocal[i * strideOut + j] = envelope;
      }
      envelopeStates[i] = (float) envelope;
   }
   return;
}

template void AUDIOLIB_envelopeDetector_exec_cn<float>(void  *pIn,
                                                       void  *pOut,
                                                       size_t widthIn,
                                                       size_t heightIn,
                                                       size_t strideIn,
                                                       size_t strideOut);
template void AUDIOLIB_envelopeDetector_exec_cn<double>(void  *pIn,
                                                        void  *pOut,
                                                        size_t widthIn,
                                                        size_t heightIn,
                                                        size_t strideIn,
                                                        size_t strideOut);

void AUDIOLIB_envelopeDetector_cn(void    *pIn,
                                  void    *pOut,
                                  uint32_t data_type,
                                  size_t   widthIn,
                                  size_t   heightIn,
                                  size_t   strideIn,
                                  size_t   strideOut)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_envelopeDetector_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_envelopeDetector_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideIn, strideOut);
   }
}
