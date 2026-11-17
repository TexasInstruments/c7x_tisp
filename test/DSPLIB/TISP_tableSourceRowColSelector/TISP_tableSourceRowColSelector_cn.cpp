// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_tableSourceRowColSelector_cn.h"
#include <dsplib.h>

template <typename dataType>
void DSPLIB_tableSourceRowColSelector_exec_cn(void    *pIn,
                                              void    *pTable,
                                              void    *pOut,
                                              size_t   numIndices,
                                              size_t   numRows,
                                              size_t   numCols,
                                              uint32_t isColSelect,
                                              uint32_t isOutputInterleave,
                                              size_t   strideOut)
{
   /* Typecast void pointers to respective data type */
   int32_t  *pInLocal    = (int32_t *) pIn;
   dataType *pTableLocal = (dataType *) pTable;
   dataType *pOutLocal   = (dataType *) pOut;

   /* Number of selectable table vectors and the length of one */
   int32_t numSel = (isColSelect == 0U) ? (int32_t) numRows : (int32_t) numCols;
   size_t  selLen = (isColSelect == 0U) ? numCols : numRows;

   for (size_t i = 0; i < numIndices; i++) {
      /* Zero-based index, clamped to the table */
      int32_t sel = pInLocal[i];
      sel         = (sel < 0) ? 0 : ((sel > (numSel - 1)) ? (numSel - 1) : sel);
      /* Table row sel, or table column sel */
      for (size_t j = 0; j < selLen; j++) {
         dataType value = (isColSelect == 0U) ? pTableLocal[sel * numCols + j] : pTableLocal[j * numCols + sel];
         /* One output row per index, or one output column per index */
         if (isOutputInterleave == 0U) {
            pOutLocal[i * strideOut + j] = value;
         }
         else {
            pOutLocal[j * strideOut + i] = value;
         }
      }
   }
   return;
}

void DSPLIB_tableSourceRowColSelector_cn(void    *pIn,
                                         void    *pTable,
                                         void    *pOut,
                                         uint32_t data_type,
                                         size_t   numIndices,
                                         size_t   numRows,
                                         size_t   numCols,
                                         uint32_t isColSelect,
                                         uint32_t isOutputInterleave,
                                         size_t   strideOut)
{
   if (data_type == DSPLIB_INT32) {
      DSPLIB_tableSourceRowColSelector_exec_cn<int32_t>(pIn, pTable, pOut, numIndices, numRows, numCols, isColSelect,
                                                        isOutputInterleave, strideOut);
   }
   else if (data_type == DSPLIB_INT64) {
      DSPLIB_tableSourceRowColSelector_exec_cn<int64_t>(pIn, pTable, pOut, numIndices, numRows, numCols, isColSelect,
                                                        isOutputInterleave, strideOut);
   }
   else if (data_type == DSPLIB_UINT32) {
      DSPLIB_tableSourceRowColSelector_exec_cn<uint32_t>(pIn, pTable, pOut, numIndices, numRows, numCols, isColSelect,
                                                         isOutputInterleave, strideOut);
   }
   else if (data_type == DSPLIB_UINT64) {
      DSPLIB_tableSourceRowColSelector_exec_cn<uint64_t>(pIn, pTable, pOut, numIndices, numRows, numCols, isColSelect,
                                                         isOutputInterleave, strideOut);
   }
   else if (data_type == DSPLIB_FLOAT32) {
      DSPLIB_tableSourceRowColSelector_exec_cn<float>(pIn, pTable, pOut, numIndices, numRows, numCols, isColSelect,
                                                      isOutputInterleave, strideOut);
   }
   else if (data_type == DSPLIB_FLOAT64) {
      DSPLIB_tableSourceRowColSelector_exec_cn<double>(pIn, pTable, pOut, numIndices, numRows, numCols, isColSelect,
                                                       isOutputInterleave, strideOut);
   }
}
