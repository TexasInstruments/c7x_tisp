// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_gainOffset.hpp"

void AUDIOLIB_gainOffset_cn(void    *pIn,
                            void    *pOut,
                            uint32_t data_type,
                            double   gain,
                            double   offset,
                            size_t   widthIn,
                            size_t   heightIn,
                            size_t   strideIn,
                            size_t   strideOut);
