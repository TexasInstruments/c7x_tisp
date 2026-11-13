// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>

#include "TISP_ifftr1dBatched.hpp"

/* Natural C of IFFTR1dBatched. All buffers hold float data.
 *   pW   : scratch, at least 4 * numPoints floats (twiddles)
 *   pSf  : scratch, at least 2 * numPoints floats (split factors)
 *   pXFFT: scratch, at least numPoints * numChannels floats (intermediate complex data)
 */
void TISP_ifftr1dBatched_cn(float   *pX,
                            float   *pW,
                            float   *pSf,
                            float   *pXFFT,
                            float   *pY,
                            uint32_t numPoints,
                            uint32_t numChannels,
                            bool     fullSpectrum);
