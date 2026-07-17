// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../DSPLIB/TISP_interleave/TISP_interleave_cn.h"
#include "TISP_asrc.hpp"

#define AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS (64U)
#define AUDIOLIB_ASRC_DOUBLE_BUFFERING_FACTOR (2U)
#define AUDIOLIB_ASRC_QUADRUPLE_BUFFERING_FACTOR (4U)
#define AUDIOLIB_ASRC_MAX_IO_SAMPLE_RATE_RATIO ((48.0f / 32.0f) * 1.1f)
#define AUDIOLIB_ASRC_FIXEDPOINT_Q28 (1UL << 28)
#define AUDIOLIB_ASRC_INPUT_INCREMENT_MASK 0x70000000UL
#define AUDIOLIB_ASRC_INPUT_INCREMENT_SHIFT 28U
#define AUDIOLIB_ASRC_MAX_MODULO_FACTOR (32U)

typedef struct TISP_asrcStateParams_t {
   volatile double outputsPerInputRatio = 1.0;
   double          fracOutputsRemaining = 0.0;
   int32_t         remBufCount          = 0;
   uint32_t        cirBuffStartIdx;
   uint32_t        outputSampleCount;
} TISP_asrcStateParams;
bool AUDIOLIB_asrc_cn_set(uint8_t               mode,
                          double                asrcRatio,
                          void                 *pNonInterleavedData,
                          void                 *pIn,
                          uint32_t              datatype,
                          uint32_t              isInterleave,
                          size_t                maxSampleCountPerBlock,
                          uint32_t              heightIn,
                          TISP_asrcStateParams *asrcStateParams);

void AUDIOLIB_asrc_cn(void                 *pInL,
                      void                 *pInR,
                      void                 *pOutL,
                      void                 *pOutR,
                      void                 *pOut,
                      size_t                widthOut,
                      size_t                heightIn,
                      TISP_asrcStateParams *asrcStateParams,
                      uint32_t              inputSampleCnt,
                      double                maxOutputsPerInputRatio,
                      size_t                strideInElements,
                      size_t                strideOutElements,
                      size_t                isInterleaved,
                      size_t                filterRembufWidthIn,
                      uint32_t              inBufferTotalDimX,
                      size_t                frameModuloFactor,
                      uint32_t              data_type);
