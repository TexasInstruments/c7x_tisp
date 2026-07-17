// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_split.hpp"

void AUDIOLIB_split_cn(void           *pIn,
                       void          **pOut,
                       uint32_t        dataType,
                       uint32_t        numOutputs,
                       uint32_t        inSamples,
                       const uint32_t *outChannels,        // per-output channel counts
                       uint32_t        strideInElements,
                       const uint32_t *strideOutElements,  // per-output output stride (elements)
                       uint8_t         isInterleave);

/* ======================================================================== */
/*  End of file:  TISP_split_cn.h                                          */
/* ======================================================================== */