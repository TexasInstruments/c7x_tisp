// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_fir.hpp"

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
                   size_t          Q);
