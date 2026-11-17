// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_tableSourceRowColSelector.hpp"

void DSPLIB_tableSourceRowColSelector_cn(void    *pIn,
                                         void    *pTable,
                                         void    *pOut,
                                         uint32_t data_type,
                                         size_t   numIndices,
                                         size_t   numRows,
                                         size_t   numCols,
                                         uint32_t isColSelect,
                                         uint32_t isOutputInterleave,
                                         size_t   strideOut);
