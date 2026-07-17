// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_LOG10_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_LOG10_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <mathlib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   void    *staticIn;
   void    *staticOut;
   uint32_t dataType;
   uint32_t length;
   uint32_t blockWidth;
   uint32_t testID;
} TISP_log10_superNode_testParams_t;

void TISP_log10_superNode_getTestParams(TISP_log10_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_LOG10_SUPERNODE_IXX_IXX_OXX_IDAT_H */
