// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_tableSourceRowColSelector.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename T>
TableSourceRowColSelector<T>::TableSourceRowColSelector(
    const TISP_BufDescList                          *pInputs,
    const TISP_BufDescList                          *pOutputs,
    void                                            *pScratch,
    size_t                                           scratchBytes,
    const TableSourceRowColSelectorConfig           *pConfig,
    const TableSourceRowColSelectorTunableParams<T> *pDefaultTunableParams,
    const char                                      *nodeName,
    uint32_t                                         nodeId,
    ErrorCtxt                                       &errorCtx)
    : _in{}, _out{}, _table(nullptr), _scratch(pScratch)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("TableSourceRowColSelector constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("TableSourceRowColSelector constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pTable == nullptr) {
      errorCtx.setNullPointerError("TableSourceRowColSelector constructor",
                                   "table pointer pDefaultTunableParams->pTable is null");
   }
   else {
      _table = pDefaultTunableParams->pTable;
   }

   // Buffer checks (skipped once an error is set): one input of int32_t indices, one output of T
   if (TISP_checkBufDescList<int32_t>(pInputs, 1U, 1U, "TableSourceRowColSelector constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "TableSourceRowColSelector constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      // Length of the table row or column selected by an index
      const size_t selLen = (pConfig->isColSelect == 0U) ? pConfig->numCols : pConfig->numRows;

      if ((_in.height > 1U) && (_in.pitchBytes != (_in.width * sizeof(int32_t)))) {
         errorCtx.setInvalidParameterError("TableSourceRowColSelector constructor", "input rows must be packed");
      }
      else if ((pConfig->numRows == 0U) || (pConfig->numCols == 0U)) {
         errorCtx.setInvalidParameterError("TableSourceRowColSelector constructor",
                                           "numRows and numCols must be greater than zero");
      }
      else if ((_out.isInterleave == 1U) && (pScratch == nullptr)) {
         errorCtx.setNullPointerError("TableSourceRowColSelector constructor",
                                      "scratch pointer pScratch is null, the interleaved output needs it");
      }
      else if ((_out.isInterleave == 1U) && (scratchBytes < calcScratchSize(_in.width * _in.height, selLen))) {
         errorCtx.setInvalidParameterError("TableSourceRowColSelector constructor",
                                           "scratchBytes is smaller than calcScratchSize(numIndices, selLen)");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      const size_t numIndices = _in.width * _in.height;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "numIndices=%zu,numRows=%zu,numCols=%zu,isColSelect=%u,isInterleave=%u", numIndices,
                      pConfig->numRows, pConfig->numCols, static_cast<uint32_t>(pConfig->isColSelect),
                      static_cast<uint32_t>(_out.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      DSPLIB_tableSourceRowColSelector_InitArgs kerInitArgs;
      kerInitArgs.funcStyle          = DSPLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isColSelect        = pConfig->isColSelect;
      kerInitArgs.isOutputInterleave = _out.isInterleave;

      _handle = make_unique_handle<char>(DSPLIB_tableSourceRowColSelector_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("TableSourceRowColSelector constructor",
                                 "failed to allocate tableSourceRowColSelector handle");
      }
      else {
         DSPLIB_bufParams1D_t bufParamsIn0;
         DSPLIB_bufParams2D_t bufParamsIn1;

         bufParamsIn0.data_type = static_cast<uint32_t>(DSPLIB_element_dType<int32_t>::value);
         bufParamsIn0.dim_x     = static_cast<uint32_t>(numIndices);

         // The table is packed: numRows rows of numCols entries
         bufParamsIn1.data_type = static_cast<uint32_t>(DSPLIB_element_dType<T>::value);
         bufParamsIn1.dim_x     = static_cast<uint32_t>(pConfig->numCols);
         bufParamsIn1.dim_y     = static_cast<uint32_t>(pConfig->numRows);
         bufParamsIn1.stride_y  = static_cast<int32_t>(pConfig->numCols * sizeof(T));

         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<T>(_out);

         DSPLIB_STATUS status = DSPLIB_tableSourceRowColSelector_init_checkParams(
             _handle.get(), &bufParamsIn0, &bufParamsIn1, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_tableSourceRowColSelector_init_checkParams")) {
            status = DSPLIB_tableSourceRowColSelector_init(_handle.get(), &bufParamsIn0, &bufParamsIn1, &bufParamsOut,
                                                           &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_tableSourceRowColSelector_init");
         }
      }
   }
}

template <typename T> size_t TableSourceRowColSelector<T>::calcScratchSize(size_t numIndices, size_t selLen) noexcept
{
   constexpr size_t lanes = TISP_SE_SA_ALIGN_BYTES / sizeof(T);

   return (((numIndices + lanes - 1U) / lanes) * lanes) * (((selLen + lanes - 1U) / lanes) * lanes) * sizeof(T);
}

template <typename T> void *TableSourceRowColSelector<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t TableSourceRowColSelector<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *TableSourceRowColSelector<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t TableSourceRowColSelector<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void TableSourceRowColSelector<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void TableSourceRowColSelector<T>::exec() noexcept
{
   DSPLIB_tableSourceRowColSelector_exec(_handle.get(), _in.pBuf, _table, _out.pBuf, _scratch);
}

template class TableSourceRowColSelector<int32_t>;
template class TableSourceRowColSelector<int64_t>;
template class TableSourceRowColSelector<uint32_t>;
template class TableSourceRowColSelector<uint64_t>;
template class TableSourceRowColSelector<float>;
template class TableSourceRowColSelector<double>;

} // namespace DSPLIB
} // namespace TISP
