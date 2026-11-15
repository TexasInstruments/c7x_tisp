// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_TABLESOURCEROWCOLSELECTOR_SUPERNODE_IXX_IXX_OXX_IDAT_H

#define TISP_TABLESOURCEROWCOLSELECTOR_SUPERNODE_IXX_IXX_OXX_IDAT_H

#include <dsplib.h>

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
   uint32_t inWidth;  // indices per input row
   uint32_t inHeight; // input rows
   int32_t  strideIn; // input row pitch in bytes
   uint32_t numRows;
   uint32_t numCols;
   uint32_t isColSelect;        // 0: an index selects a table row, 1: a table column
   uint32_t isOutputInterleave; // 0: one output row per index, 1: one output column per index
   uint32_t blockWidth;         // indices per tile
   int32_t  outPitch;           // output row pitch in bytes
   uint32_t testID;
} TISP_tableSourceRowColSelector_superNode_testParams_t;

void TISP_tableSourceRowColSelector_superNode_getTestParams(
    TISP_tableSourceRowColSelector_superNode_testParams_t **params,
    int32_t                                                *numTests);

#ifdef __cplusplus
}
#endif

#endif /* define TISP_TABLESOURCEROWCOLSELECTOR_SUPERNODE_IXX_IXX_OXX_IDAT_H */
