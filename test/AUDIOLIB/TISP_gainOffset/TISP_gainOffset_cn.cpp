// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_gainOffset_cn.h"
#include <audiolib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

/*
 * Per-type arithmetic of the reference, matching AUDIOLIB_gainOffset:
 *   paramType : type of gain / offset
 *   accType   : type gain * x + offset is computed in
 *
 *   | dataType | paramType | accType |
 *   |----------|-----------|---------|
 *   | float    | float     | float   |
 *   | double   | float     | double  |
 *   | int32_t  | int32_t   | int64_t | (saturated once to int32)
 */
template <typename dataType> struct gainOffsetCnTypes;

template <> struct gainOffsetCnTypes<float> {
   typedef float paramType;
   typedef float accType;
};

template <> struct gainOffsetCnTypes<double> {
   typedef float  paramType;
   typedef double accType;
};

template <> struct gainOffsetCnTypes<int32_t> {
   typedef int32_t paramType;
   typedef int64_t accType;
};

static inline float gainOffsetCnToOut(float acc) { return acc; }

static inline double gainOffsetCnToOut(double acc) { return acc; }

static inline int32_t gainOffsetCnToOut(int64_t acc)
{
   int64_t sat = acc;

   if (sat > (int64_t) INT32_MAX) {
      sat = (int64_t) INT32_MAX;
   }
   else if (sat < (int64_t) INT32_MIN) {
      sat = (int64_t) INT32_MIN;
   }

   return (int32_t) sat;
}

template <typename dataType>
void AUDIOLIB_gainOffset_exec_cn(void  *pIn,
                                 void  *pOut,
                                 double gain,
                                 double offset,
                                 size_t widthIn,
                                 size_t heightIn,
                                 size_t strideIn,
                                 size_t strideOut)
{
   typedef typename gainOffsetCnTypes<dataType>::paramType paramType;
   typedef typename gainOffsetCnTypes<dataType>::accType   accType;

   /* Typecast void pointers to respective data type */
   const dataType *pInLocal  = (const dataType *) pIn;
   dataType       *pOutLocal = (dataType *) pOut;

   /* gain and offset are rounded to the parameter type first, as the kernel stores them in its handle */
   const accType gainLocal   = (accType) (paramType) gain;
   const accType offsetLocal = (accType) (paramType) offset;

   for (uint32_t i = 0; i < heightIn; i++) {
      for (uint32_t j = 0; j < widthIn; j++) {
         const accType acc = ((accType) pInLocal[i * strideIn + j] * gainLocal) + offsetLocal;

         pOutLocal[i * strideOut + j] = gainOffsetCnToOut(acc);
      }
   }
}

void AUDIOLIB_gainOffset_cn(void    *pIn,
                            void    *pOut,
                            uint32_t data_type,
                            double   gain,
                            double   offset,
                            size_t   widthIn,
                            size_t   heightIn,
                            size_t   strideIn,
                            size_t   strideOut)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      AUDIOLIB_gainOffset_exec_cn<float>(pIn, pOut, gain, offset, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      AUDIOLIB_gainOffset_exec_cn<double>(pIn, pOut, gain, offset, widthIn, heightIn, strideIn, strideOut);
   }
   else if (data_type == AUDIOLIB_INT32) {
      AUDIOLIB_gainOffset_exec_cn<int32_t>(pIn, pOut, gain, offset, widthIn, heightIn, strideIn, strideOut);
   }
}
