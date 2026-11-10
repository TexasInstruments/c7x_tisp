// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_whiteNoise_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

extern uint32_t seedCn;

template <typename dataType>
void AUDIOLIB_whiteNoise_exec_cn(void *pOut, size_t widthOut, size_t numChannels, float range)
{
   /* Typecast void pointers to respective data type */
   dataType *RESTRICT pOutLocal = (dataType *) pOut;

   // Standard LCG parameters
   const uint32_t a = 1664525u;
   const uint32_t c = 1013904223u;
   // m = 2**32 is implicit in uint32_t arithmetic wrap-around

   // The LCG advanced by 51274189 steps (the kernel's channel spacing): x_n+51274189 = channelA * x_n + channelC
   const uint32_t channelA = 1417596669u;
   const uint32_t channelC = 719848139u;

   // range / 2**31 maps the zero-centred LCG output to [-range, range]
   const dataType scale = (dataType) range * (dataType) 4.656612873077393e-10;

   // Channel 0 starts from the seed; every other channel 51274189 steps after the previous one
   uint32_t channelSeed = seedCn;

   for (size_t ch = 0; ch < numChannels; ch++) {
      uint32_t x = channelSeed;

      for (uint32_t i = 0; i < widthOut; i++) {
         // 1. Generate the next pseudo-random integer: x = (a * x + c) % m
         // The modulo is implicit for uint32_t.
         x = (a * x + c);

         // 2. Centre on zero and scale to the desired range [-range, range]: (x - 2**31) / 2**31 * range
         pOutLocal[ch * widthOut + i] = (dataType) (int32_t) (x ^ 0x80000000u) * scale;
      }

      // Update the seed for the next exec call from channel 0, so every channel's sequence continues
      if (ch == 0) {
         seedCn = x;
      }
      channelSeed = channelA * channelSeed + channelC;
   }
   return;
}

template void AUDIOLIB_whiteNoise_exec_cn<float>(void *pOut, size_t widthOut, size_t numChannels, float range);

void AUDIOLIB_whiteNoise_cn(void *pOut, uint32_t data_type, size_t widthOut, size_t numChannels, float range)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_whiteNoise_exec_cn<float>(pOut, widthOut, numChannels, range);
   }
}
