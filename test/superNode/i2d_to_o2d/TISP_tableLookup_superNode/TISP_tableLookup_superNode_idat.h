// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_TABLELOOKUP_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_TABLELOOKUP_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <audiolib.h>

// include test infrastructure provided by TISP
#include "../common/TISP_test.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t  testPattern;
   void    *staticIn0;
   void    *staticIn1;
   void    *staticOut;
   uint32_t dataType;
   uint32_t srcSamples;
   uint32_t tableSamples;
   float    minVal;
   float    maxVal;
   uint32_t tableLookupSize;
   uint32_t blockWidth;
   uint32_t testID;
} TISP_tableLookup_superNode_testParams_t;

void TISP_tableLookup_superNode_getTestParams(TISP_tableLookup_superNode_testParams_t **params, int32_t *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_TABLELOOKUP_SUPERNODE_IXX_IXX_OXX_IDAT_H */
