// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_typeConversion_cn.h"
#include "common/AUDIOLIB_bufParams.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dataType>
void AUDIOLIB_q15ToFloat_exec_cn(void  *pIn,
                                 void  *pOut,
                                 size_t widthIn,
                                 size_t heightIn,
                                 size_t strideInElements,
                                 size_t strideOutElements)
{
   /* Typecast void pointers to respective data type */
   int16_t  *pInLocal  = (int16_t *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t m = 0; m < heightIn; m++) {
      for (uint32_t n = 0; n < widthIn; n++) {
         pOutLocal[m * strideOutElements + n] = (dataType) pInLocal[m * strideInElements + n] * (1 / 32768.0);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_floatToQ15_exec_cn(void  *pIn,
                                 void  *pOut,
                                 size_t widthIn,
                                 size_t heightIn,
                                 size_t strideInElements,
                                 size_t strideOutElements)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   int16_t  *pOutLocal = (int16_t *) pOut;

   dataType mulFactor = 32768.00;
   int32_t  aInt      = 0;
   int16_t  out       = 0;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType mulVal   = pInLocal[i * strideInElements + j] * mulFactor;
         int32_t  intPart  = (int32_t) mulVal;
         dataType fracPart = mulVal - intPart;

         if ((intPart % 2 == 0) && (abs(fracPart) == 0.5)) {
            aInt = intPart;
         }
         else if (pInLocal[i * strideInElements + j] < 0) {
            aInt = (int32_t) (mulVal - 0.5f);
         }
         else {
            aInt = (int32_t) (mulVal + 0.5f);
         }

         /* saturate to 16-bit */
         if (aInt > 32767) {
            aInt = 32767;
         }
         if (aInt < -32768) {
            aInt = -32768;
         }
         out = (short) aInt;

         pOutLocal[i * strideOutElements + j] = out;
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_q24ToFloat_exec_cn(void  *pIn,
                                 void  *pOut,
                                 size_t widthIn,
                                 size_t heightIn,
                                 size_t strideInElements,
                                 size_t strideOutElements)
{
   /* Typecast void pointers to respective data type */
   int32_t  *pInLocal  = (int32_t *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t m = 0; m < heightIn; m++) {
      for (uint32_t n = 0; n < widthIn; n++) {
         pOutLocal[m * strideOutElements + n] = (dataType) pInLocal[m * strideInElements + n] * (1 / 8388608.0);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_q31ToFloat_exec_cn(void  *pIn,
                                 void  *pOut,
                                 size_t widthIn,
                                 size_t heightIn,
                                 size_t strideInElements,
                                 size_t strideOutElements)
{
   int32_t  *pInLocal  = (int32_t *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         pOutLocal[i * strideOutElements + j] = (dataType) (pInLocal[i * strideInElements + j] * (1 / 2147483648.0));
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_floatToQ24_exec_cn(void  *pIn,
                                 void  *pOut,
                                 size_t widthIn,
                                 size_t heightIn,
                                 size_t strideInElements,
                                 size_t strideOutElements)
{
   /* Typecast void pointers to respective data type */
   dataType *pInLocal  = (dataType *) pIn;
   int32_t  *pOutLocal = (int32_t *) pOut;

   dataType mulFactor = 8388608.0;
   int32_t  aInt      = 0;
   int32_t  out       = 0;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType mulVal   = pInLocal[i * strideInElements + j] * mulFactor;
         int32_t  intPart  = (int32_t) mulVal;
         dataType fracPart = mulVal - intPart;

         if ((intPart % 2 == 0) && (abs(fracPart) == 0.5)) {
            aInt = intPart;
         }
         else if (pInLocal[i * strideInElements + j] < 0) {
            aInt = (int32_t) (mulVal - 0.5f);
         }
         else {
            aInt = (int32_t) (mulVal + 0.5f);
         }

         /* saturate to 16-bit */
         if (aInt > 8388607) {
            aInt = 8388607;
         }
         if (aInt < -8388608) {
            aInt = -8388608;
         }
         out = (int32_t) aInt;

         pOutLocal[i * strideOutElements + j] = out;
      }
   }
   return;
}

template <typename dataType>
void AUDIOLIB_floatToQ31_exec_cn(void  *pIn,
                                 void  *pOut,
                                 size_t widthIn,
                                 size_t heightIn,
                                 size_t strideInElements,
                                 size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   int32_t  *pOutLocal = (int32_t *) pOut;

   dataType mulFactor = 2147483648;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         dataType val    = pInLocal[i * strideInElements + j];
         double   mulVal = static_cast<double>(val) * mulFactor; // promote to double

         // --- Handle NaN/Inf manually ---
         if (!(mulVal == mulVal) || (mulVal > 1e308) || (mulVal < -1e308)) {
            pOutLocal[i * strideOutElements + j] = 0;
            continue;
         }

         // --- Saturate before rounding ---
         if (mulVal >= (double) INT32_MAX)
            mulVal = (double) INT32_MAX;
         else if (mulVal <= (double) INT32_MIN)
            mulVal = (double) INT32_MIN;

         // --- Round to nearest, ties to even ---
         double floorVal = floor(mulVal);
         double diff     = mulVal - floorVal;
         double rounded;
         if (diff > 0.5)
            rounded = floorVal + 1.0;
         else if (diff < 0.5)
            rounded = floorVal;
         else // diff == 0.5
            rounded = (fmod(floorVal, 2.0) == 0.0) ? floorVal : floorVal + 1.0;

         // --- Final integer saturation ---
         int64_t intVal = static_cast<int64_t>(rounded);
         if (intVal > INT32_MAX)
            intVal = INT32_MAX;
         else if (intVal < INT32_MIN)
            intVal = INT32_MIN;

         pOutLocal[i * strideOutElements + j] = static_cast<int32_t>(intVal);
      }
   }
   return;
}

template void AUDIOLIB_q15ToFloat_exec_cn<float>(void  *pIn,
                                                 void  *pOut,
                                                 size_t widthIn,
                                                 size_t heightIn,
                                                 size_t strideInElements,
                                                 size_t strideOutElements);
template void AUDIOLIB_q15ToFloat_exec_cn<double>(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t strideInElements,
                                                  size_t strideOutElements);

template void AUDIOLIB_floatToQ15_exec_cn<float>(void  *pIn,
                                                 void  *pOut,
                                                 size_t widthIn,
                                                 size_t heightIn,
                                                 size_t strideInElements,
                                                 size_t strideOutElements);
template void AUDIOLIB_floatToQ15_exec_cn<double>(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t strideInElements,
                                                  size_t strideOutElements);

template void AUDIOLIB_q24ToFloat_exec_cn<float>(void  *pIn,
                                                 void  *pOut,
                                                 size_t widthIn,
                                                 size_t heightIn,
                                                 size_t strideInElements,
                                                 size_t strideOutElements);
template void AUDIOLIB_q24ToFloat_exec_cn<double>(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t strideInElements,
                                                  size_t strideOutElements);

template void AUDIOLIB_floatToQ24_exec_cn<float>(void  *pIn,
                                                 void  *pOut,
                                                 size_t widthIn,
                                                 size_t heightIn,
                                                 size_t strideInElements,
                                                 size_t strideOutElements);
template void AUDIOLIB_floatToQ24_exec_cn<double>(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t strideInElements,
                                                  size_t strideOutElements);
template void AUDIOLIB_q31ToFloat_exec_cn<float>(void  *pIn,
                                                 void  *pOut,
                                                 size_t widthIn,
                                                 size_t heightIn,
                                                 size_t strideInElements,
                                                 size_t strideOutElements);
template void AUDIOLIB_q31ToFloat_exec_cn<double>(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t strideInElements,
                                                  size_t strideOutElements);
template void AUDIOLIB_floatToQ31_exec_cn<float>(void  *pIn,
                                                 void  *pOut,
                                                 size_t widthIn,
                                                 size_t heightIn,
                                                 size_t strideInElements,
                                                 size_t strideOutElements);
template void AUDIOLIB_floatToQ31_exec_cn<double>(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t strideInElements,
                                                  size_t strideOutElements);

void AUDIOLIB_typeConversion_cn(void    *pIn,
                                void    *pOut,
                                uint32_t indataType,
                                uint32_t outdataType,
                                size_t   widthIn,
                                size_t   heightIn,
                                size_t   strideInElements,
                                size_t   strideOutElements,
                                size_t   testQ31)
{
   if (indataType == AUDIOLIB_INT16) {
      if (outdataType == AUDIOLIB_FLOAT32) {
         AUDIOLIB_q15ToFloat_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (outdataType == AUDIOLIB_FLOAT64) {
         AUDIOLIB_q15ToFloat_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
   }
   else if (outdataType == AUDIOLIB_INT16) {
      if (indataType == AUDIOLIB_FLOAT32) {
         AUDIOLIB_floatToQ15_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (indataType == AUDIOLIB_FLOAT64) {
         AUDIOLIB_floatToQ15_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
   }
   else if (indataType == AUDIOLIB_INT32) {
      if (outdataType == AUDIOLIB_FLOAT32 && testQ31 == 0) {
         AUDIOLIB_q24ToFloat_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      if (outdataType == AUDIOLIB_FLOAT32 && testQ31 == 1) {
         AUDIOLIB_q31ToFloat_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (outdataType == AUDIOLIB_FLOAT64 && testQ31 == 0) {
         AUDIOLIB_q24ToFloat_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (outdataType == AUDIOLIB_FLOAT64 && testQ31 == 1) {
         AUDIOLIB_q31ToFloat_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
   }
   else if (outdataType == AUDIOLIB_INT32) {
      if (indataType == AUDIOLIB_FLOAT32 && testQ31 == 0) {
         AUDIOLIB_floatToQ24_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (indataType == AUDIOLIB_FLOAT64 && testQ31 == 0) {
         AUDIOLIB_floatToQ24_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      if (indataType == AUDIOLIB_FLOAT32 && testQ31 == 1) {
         AUDIOLIB_floatToQ31_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (indataType == AUDIOLIB_FLOAT64 && testQ31 == 1) {
         AUDIOLIB_floatToQ31_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
   }
}
