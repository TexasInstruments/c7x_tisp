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

template <typename dataType> void AUDIOLIB_whiteNoise_exec_cn(void *pOut, size_t widthOut, float range)
{
   /* Typecast void pointers to respective data type */
   dataType *RESTRICT pOutLocal = (dataType *) pOut;

   // Standard LCG parameters
   const uint32_t a = 1664525u;
   const uint32_t c = 1013904223u;
   // m = 2**32 is implicit in uint32_t arithmetic wrap-around

   // Pre-calculate 1.0 / (2**32) for normalization
   const dataType m_inv = (dataType) 2.3283064365386963e-10;

   // Initialize LCG state from the handle
   uint32_t x = seedCn;

   for (uint32_t i = 0; i < widthOut; i++) {
      // 1. Generate the next pseudo-random integer: x = (a * x + c) % m
      // The modulo is implicit for uint32_t.
      x = (a * x + c);

      // 2. Normalize the integer to [0.0, 1.0)
      // normalized_val = (dataType)x / (double)(1ULL << 32)
      dataType normalized_val = (dataType) x * m_inv;

      // 3. Scale to the desired range [-range, range]
      // out[i] = (2 * range_val * normalized_val) - range_val
      pOutLocal[i] = (2.0f * range * normalized_val) - range;
   }

   // Update the seed in the handle for the next exec call
   // This ensures the noise sequence continues from where it left off.
   seedCn = x;
   return;
}

template void AUDIOLIB_whiteNoise_exec_cn<float>(void *pOut, size_t widthOut, float range);

void AUDIOLIB_whiteNoise_cn(void *pOut, uint32_t data_type, size_t widthOut, float range)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_whiteNoise_exec_cn<float>(pOut, widthOut, range);
   }
}
