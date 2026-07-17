// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_tableInterpolate.hpp"

void AUDIOLIB_tableInterpolate_cn(void    *pIn,
                                  void    *pTableinterpolate,
                                  void    *pOut,
                                  uint32_t data_type,
                                  size_t   srcSamples,
                                  size_t   tableSamples,
                                  float    minVal,
                                  float    divisor);
