// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <stdbool.h>
#include <stdint.h>

#include "TISP_stft.hpp"

void AUDIOLIB_stft_cn(float    *pX,
                      float    *pY,
                      float    *pWindow,
                      float    *pState,
                      float    *pScratch,
                      uint32_t  fftSize,
                      uint32_t  hopSize,
                      uint32_t  numChannels,
                      uint32_t  numInputSamples,
                      bool      onesided);
