// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_tableLookup_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_tableLookup_exec_cn(void  *pIn,
                                  void  *pTablelookup,
                                  void  *pOut,
                                  size_t srcSamples,
                                  size_t tableSamples,
                                  float  minVal,
                                  float  divisor)
{
   /* Typecast void pointers to respective data type */
   dataType *restrict pInLocalSrc   = (dataType *) pIn;
   dataType *restrict pInLocalTable = (dataType *) pTablelookup;
   dataType *restrict pOutLocalOut  = (dataType *) pOut;

   for (uint32_t i = 0; i < srcSamples; i++) {
      dataType sampleSrc = pInLocalSrc[i];
      uint32_t index     = 0;

      sampleSrc -= minVal;
      sampleSrc *= divisor;
      sampleSrc = sampleSrc + 0.5f;

      if (sampleSrc <= 0) {
         pOutLocalOut[i] = pInLocalTable[0];
      }
      else if (sampleSrc >= (tableSamples - 1)) {

         pOutLocalOut[i] = pInLocalTable[tableSamples - 1];
      }
      else {
         index           = floorf(sampleSrc);
         pOutLocalOut[i] = pInLocalTable[index];
      }
   }
   return;
}

template void AUDIOLIB_tableLookup_exec_cn<float>(void  *pIn,
                                                  void  *pTablelookup,
                                                  void  *pOut,
                                                  size_t srcSamples,
                                                  size_t tableSamples,
                                                  float  minVal,
                                                  float  divisor);

template <typename tableType>
void AUDIOLIB_tableLookupInt_exec_cn(void *pIn, void *pTablelookup, void *pOut, size_t srcSamples, size_t tableSamples)
{
   /* Typecast void pointers to respective data type */
   int32_t *restrict pInLocalSrc     = (int32_t *) pIn;
   tableType *restrict pInLocalTable = (tableType *) pTablelookup;
   tableType *restrict pOutLocalOut  = (tableType *) pOut;

   for (uint32_t i = 0; i < srcSamples; i++) {
      int32_t index = pInLocalSrc[i];

      if (index <= 0) {
         pOutLocalOut[i] = pInLocalTable[0];
      }
      else if (index >= (int32_t) (tableSamples - 1)) {
         pOutLocalOut[i] = pInLocalTable[tableSamples - 1];
      }
      else {
         pOutLocalOut[i] = pInLocalTable[index];
      }
   }
   return;
}

template void AUDIOLIB_tableLookupInt_exec_cn<float>(void  *pIn,
                                                     void  *pTablelookup,
                                                     void  *pOut,
                                                     size_t srcSamples,
                                                     size_t tableSamples);
template void AUDIOLIB_tableLookupInt_exec_cn<int32_t>(void  *pIn,
                                                       void  *pTablelookup,
                                                       void  *pOut,
                                                       size_t srcSamples,
                                                       size_t tableSamples);

void AUDIOLIB_tableLookup_cn(void    *pIn,
                             void    *pTablelookup,
                             void    *pOut,
                             uint32_t data_type,
                             uint32_t table_data_type,
                             size_t   srcSamples,
                             size_t   tableSamples,
                             float    minVal,
                             float    divisor)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_tableLookup_exec_cn<float>(pIn, pTablelookup, pOut, srcSamples, tableSamples, minVal, divisor);
   }
   else if ((data_type == AUDIOLIB_INT32) && (table_data_type == AUDIOLIB_FLOAT32)) {
      AUDIOLIB_tableLookupInt_exec_cn<float>(pIn, pTablelookup, pOut, srcSamples, tableSamples);
   }
   else if ((data_type == AUDIOLIB_INT32) && (table_data_type == AUDIOLIB_INT32)) {
      AUDIOLIB_tableLookupInt_exec_cn<int32_t>(pIn, pTablelookup, pOut, srcSamples, tableSamples);
   }
}
