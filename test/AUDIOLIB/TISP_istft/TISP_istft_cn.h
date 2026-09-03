// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <stdbool.h>
#include <stdint.h>

#include "TISP_istft.hpp"

void AUDIOLIB_istft_cn(float    *pX,
                       float    *pY,
                       float    *pWindow,
                       float    *pOlaBuf,
                       uint32_t  fftSize,
                       uint32_t  hopSize,
                       uint32_t  numChannels,
                       uint32_t  numWindows,
                       bool      onesided);
