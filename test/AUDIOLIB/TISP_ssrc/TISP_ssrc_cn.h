// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_ssrc.hpp"

#define DSPLIB_MMA_SIZE_32_BIT __MMA_A_COLS(sizeof(int32_t))

#define AUDIOLIB_SSRC_MATRIX_TRANSPOSE_ROW_STRIDE(x, y) (((x + y - 1) / y) * y)

#define AUDIOLIB_SSRC_DOWNSAMPLING_STAGE1_FILTER_TAPS (AUDIOLIB_SSRC_DOWNSAMPLING_STAGE1_ORIGINAL_FILTER_TAPS + 1U) / 4U

#define AUDIOLIB_SSRC_UPSAMPLING_STAGE2_FILTER_TAPS (AUDIOLIB_SSRC_UPSAMPLING_STAGE2_ORIGINAL_FILTER_TAPS + 1U) / 4U

#define AUDIOLIB_SSRC_UPSAMPLING_STAGE2_FILTER_TAPS_SYMMETRIC                                                          \
   (AUDIOLIB_SSRC_UPSAMPLING_STAGE2_ORIGINAL_FILTER_TAPS + 1U) / 2U

#define AUDIOLIB_SSRC_NUMBER_OF_FILTER_PHASES (2U)

#define AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_FILTER_TAPS_SYMMETRIC                                                        \
   (AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_ORIGINAL_FILTER_TAPS + 1U) / 2U

#define AUDIOLIB_SSRC_DOWNSAMPLING_STAGE1_FILTER_TAPS_SYMMETRIC                                                        \
   (AUDIOLIB_SSRC_DOWNSAMPLING_STAGE1_ORIGINAL_FILTER_TAPS + 1U) / 2U

#define AUDIOLIB_SSRC_UPSAMPLING_STAGE1_ORIGINAL_FILTER_TAPS (191U)

#define AUDIOLIB_SSRC_UPSAMPLING_STAGE1_FILTER_TAPS (AUDIOLIB_SSRC_UPSAMPLING_STAGE1_ORIGINAL_FILTER_TAPS + 1U) / 4U

#define AUDIOLIB_SSRC_UPSAMPLING_STAGE1_FILTER_TAPS_SYMMETRIC                                                          \
   (AUDIOLIB_SSRC_UPSAMPLING_STAGE1_ORIGINAL_FILTER_TAPS + 1U) / 2U

#define AUDIOLIB_SSRC_MIN(x, y) (((x) < (y)) ? (x) : (y))

#define AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_ORIGINAL_FILTER_TAPS (191U)

#define AUDIOLIB_SSRC_PING_PONG_FACTOR (2U)

#define AUDIOLIB_SSRC_DOWNSAMPLING_STAGE1_ORIGINAL_FILTER_TAPS (39U)

#define AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_FILTER_TAPS (AUDIOLIB_SSRC_DOWNSAMPLING_STAGE2_ORIGINAL_FILTER_TAPS + 1U) / 4U

#define AUDIOLIB_ALIGN_SHIFT_64BYTES 6  /* < Number of bits to shift for 64-byte memory alignment */
#define AUDIOLIB_ALIGN_SHIFT_128BYTES 7 /* < Number of bits to shift for 128-byte memory alignment */
#define AUDIOLIB_ALIGN_SHIFT_256BYTES 8 /* < Number of bits to shift for 256-byte memory alignment */

#define AUDIOLIB_ALIGN_64BYTES (1 << AUDIOLIB_ALIGN_SHIFT_64BYTES)   /* < Align by 64-byte memory alignment */
#define AUDIOLIB_ALIGN_128BYTES (1 << AUDIOLIB_ALIGN_SHIFT_128BYTES) /* < Align by 128-byte memory alignment */
#define AUDIOLIB_ALIGN_256BYTES (1 << AUDIOLIB_ALIGN_SHIFT_256BYTES) /* < Align by 256-byte memory alignment */

#define AUDIOLIB_L2DATA_ALIGN_SHIFT AUDIOLIB_ALIGN_SHIFT_64BYTES /* < Set the default L2 data alignment */

#define AUDIOLIB_L2DATA_ALIGNMENT (((uint32_t) 1) << ((uint32_t) AUDIOLIB_L2DATA_ALIGN_SHIFT))

typedef struct TISP_ssrcStateParams_t {
   uint32_t stage1BuffStartIndex;
   uint32_t stage2BuffCurrIndex;
   uint32_t stage2BuffStartIndex;
   uint32_t inBufferTotalDimX;
   uint32_t stage1FiltCoeffsOffset;
   uint32_t stage2FiltCoeffsOffset;
} TISP_ssrcStateParams;

bool AUDIOLIB_ssrc_cn_set(void    *pIn,
                          void    *pState,
                          uint8_t  mode,
                          uint8_t  inputSampleRate,
                          uint8_t  outputSampleRate,
                          uint32_t numChannels,
                          uint32_t inBufferTotalDimX,
                          uint32_t sampleRateRatio,
                          int32_t  inputSampleCount,
                          uint8_t  bufferFormat,
                          uint32_t sampleDataType);

void AUDIOLIB_ssrc_cn(void                 *pIn,
                      void                 *pFiltCoeffs,
                      void                 *pOutCn,
                      uint32_t              widthIn,
                      uint32_t              heightIn,
                      uint32_t              cirBuffAddressMask,
                      int32_t               inputSampleCount,
                      bool                  upSampling,
                      uint32_t              sampleRateRatio,
                      int32_t               outputSampleCount,
                      TISP_ssrcStateParams *ssrcStateParams);
