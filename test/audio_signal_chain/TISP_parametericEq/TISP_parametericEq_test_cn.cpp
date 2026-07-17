// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

void DSPLIB_addConstant_exec_cn(float *RESTRICT pIn0, float *RESTRICT pIn1, float *RESTRICT pOut, size_t size)
{
   for (uint32_t i = 0; i < size; i++) {
      pOut[i] = (*pIn0) + pIn1[i];
   }
   return;
}

void DSPLIB_sqr_exec_cn(float *RESTRICT pIn, float *RESTRICT pOut, size_t size)
{
   for (uint32_t i = 0; i < size; i++) {
      pOut[i] = pIn[i] * pIn[i];
   }
   return;
}

void TISP_addConstant_sqr_test_cn(float *pIn0, float *pIn1, float *pOut0, float *pOut1, size_t size)
{

   DSPLIB_addConstant_exec_cn(pIn0, pIn1, pOut0, size);
   DSPLIB_sqr_exec_cn(pOut0, pOut1, size);

   return;
}
