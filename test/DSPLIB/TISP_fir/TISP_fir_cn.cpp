// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_fir_cn.h"
#include <cstddef>
#include <cstdint>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

template <typename dType>
void DSPLIB_fir_cn(dType *RESTRICT pIn,
                   dType *RESTRICT pState,
                   dType *RESTRICT pFilter,
                   dType *RESTRICT pOut,
                   size_t          inputSize,
                   size_t          stateSize,
                   size_t          filterSize,
                   size_t          outputSize,
                   size_t          batchSize,
                   size_t          inPitch,
                   size_t          statePitch,
                   size_t          filterPitch,
                   size_t          outPitch,
                   bool            enableNchCoefs,
                   bool            enableMMA,
                   bool            enableQ,
                   size_t          Q)
{

   typedef typename c7x::make_full_vector<dType>::type vec;

   int32_t k;
   dType   outProd, outTemp;

   int32_t dataOffset    = 0;
   int32_t dataOffsetOut = 0;
   size_t  inPitchLocal, filterPitchLocal, outPitchLocal, statePitchLocal;

   dType *pInLocal     = (dType *) pIn;
   dType *pFilterLocal = (dType *) pFilter;
   dType *pOutLocal    = (dType *) pOut;
   dType *pStateLocal  = (dType *) pState;

   uint32_t MMA_SIZE = c7x::element_count_of<vec>::value;

   inPitchLocal     = inPitch / sizeof(dType);
   filterPitchLocal = filterPitch / sizeof(dType);
   statePitchLocal  = statePitch / sizeof(dType);

   filterPitchLocal = (enableNchCoefs) ? filterPitchLocal : 0;
   filterPitchLocal = (enableMMA == 0) ? filterPitchLocal : (2 * MMA_SIZE + filterSize + MMA_SIZE - 1);

   outPitchLocal = outPitch / sizeof(dType);

   uint32_t dataBufferOutPitch = outPitchLocal;

   // copy inputSize samples from input to state buffer after filterSize delay line per channel
   for (size_t c = 0; c < batchSize; c++) {
      for (size_t n = 0; n < inputSize; n++) {

         uint32_t stateOffset = (c * statePitchLocal) + (n + filterSize - 1);
         uint32_t inputOffset = (c * inPitchLocal) + n;

         pStateLocal[stateOffset] = pInLocal[inputOffset];
      }
   }

   // fir kernel core
   for (size_t l = 0; l < batchSize; l++) {
      for (size_t j = 0; j < outputSize; j++) {
         outTemp           = 0;
         dataOffset        = l * statePitchLocal + j;
         dataOffsetOut     = l * dataBufferOutPitch + j;
         int32_t filterCtr = 0;
         for (k = filterSize - 1; k > -1; k--) {
            outProd = pStateLocal[dataOffset + filterCtr] * pFilterLocal[k];
            outTemp = outTemp + outProd;
            filterCtr++;
         }

         pOutLocal[dataOffsetOut] = outTemp;
      }

      pFilterLocal += filterPitchLocal;
   }

   // TODO: copy from state buffer to beginning of state buffer per channel

   for (size_t c = 0; c < batchSize; c++) {
      for (size_t n = 0; n < filterSize - 1; n++) {

         uint32_t dstOffset = (c * statePitchLocal) + n;
         uint32_t srcOffset = (c * statePitchLocal) + (n + inputSize);

         pStateLocal[dstOffset] = pStateLocal[srcOffset];
      }
   }
}

template void DSPLIB_fir_cn<float>(float *RESTRICT pIn,
                                   float *RESTRICT pState,
                                   float *RESTRICT pFilter,
                                   float *RESTRICT pOut,
                                   size_t          inputSize,
                                   size_t          stateSize,
                                   size_t          filterSize,
                                   size_t          outputSize,
                                   size_t          batchSize,
                                   size_t          inPitch,
                                   size_t          statePitch,
                                   size_t          filterPitch,
                                   size_t          outPitch,
                                   bool            enableNchCoefs,
                                   bool            enableMMA,
                                   bool            enableQ,
                                   size_t          Q);
