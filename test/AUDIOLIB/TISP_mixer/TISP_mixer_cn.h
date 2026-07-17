// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_mixer.hpp"

/**
 * @brief Natural C reference implementation of the audio mixer kernel.
 *
 * Aggregates multiple audio input buffers, applies coefficient-based mixing via
 * matrix multiplication, and outputs the mixed audio signal. Supports both
 * interleaved and planar audio formats.
 *
 * @param[in]  pIn                Array of pointers to input audio buffers
 * @param[out] pOut               Pointer to output buffer for mixed audio
 * @param[in]  pCoeffs            Pointer to mixing coefficient matrix [numOutputChannels x totalInChannels]
 * @param[in]  pScratch           Pointer to scratch buffer for intermediate aggregated data
 * @param[in]  pTempOut           Pointer to temporary output buffer (used for interleaved output)
 * @param[in]  numInputs          Number of input audio sources
 * @param[in]  pInChannels        Array containing the channel count for each input source
 * @param[in]  numOutputChannels  Number of output channels to produce
 * @param[in]  numSamples         Number of audio samples per channel to process
 * @param[in]  totalInChannels    Total number of input channels across all inputs
 * @param[in]  strideInElements   Input stride in elements (pitch between rows)
 * @param[in]  strideOutElements  Output stride in elements
 * @param[in]  strideScratch      Scratch buffer stride in elements
 * @param[in]  strideTempOut      Temporary output buffer stride in elements
 * @param[in]  isInterleave       Format flag: 1 = interleaved (samples x channels), 0 = planar (channels x samples)
 * @param[in]  strideCoeff        Coefficient matrix stride in elements
 * @param[in]  dataType           Data type: AUDIOLIB_FLOAT32 or AUDIOLIB_FLOAT64
 */
void AUDIOLIB_mixer_cn(void   **pIn,
                       void    *pOut,
                       void    *pCoeffs,
                       void    *pScratch,
                       void    *pTempOut,
                       int32_t  numInputs,
                       int32_t *pInChannels,
                       int32_t  numOutputChannels,
                       int32_t  numSamples,
                       int32_t  totalInChannels,
                       int32_t  strideInElements,
                       int32_t  strideOutElements,
                       int32_t  strideScratch,
                       int32_t  strideTempOut,
                       int32_t  isInterleave,
                       int32_t  strideCoeff,
                       int32_t  dataType);
