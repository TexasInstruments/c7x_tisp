// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_tableLookup.hpp"

void AUDIOLIB_tableLookup_cn(void    *pIn,
                             void    *pTablelookup,
                             void    *pOut,
                             uint32_t data_type,
                             uint32_t table_data_type,
                             size_t   srcSamples,
                             size_t   tableSamples,
                             float    minVal,
                             float    divisor);
