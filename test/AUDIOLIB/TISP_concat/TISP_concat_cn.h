// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_concat.hpp"

void AUDIOLIB_concat_cn(void          **pIn,
                        void           *pOut,
                        uint32_t        dataType,
                        uint32_t        numInputs,
                        const uint32_t *inChannels,        // per-input channel counts
                        uint32_t        inSamples,
                        const uint32_t *strideInElements,  // per-input input stride (elements)
                        uint32_t        strideOutElements,
                        uint8_t         isInterleave);

/* ======================================================================== */
/*  End of file:  TISP_concat_cn.h                                          */
/* ======================================================================== */