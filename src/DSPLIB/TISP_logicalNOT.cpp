// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_logicalNOT.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename T>
LogicalNOT<T>::LogicalNOT(const TISP_BufDescList *pInputs,
                          const TISP_BufDescList *pOutputs,
                          const char             *nodeName,
                          uint32_t                nodeId,
                          ErrorCtxt              &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "LogicalNOT constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "LogicalNOT constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_out.isInterleave != _in.isInterleave) {
         errorCtx.setInvalidParameterError("LogicalNOT constructor", "output 0 layout must match input 0");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,isInterleave=%u",
                      _in.width, _in.height, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      DSPLIB_logicalNOT_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;
      _handle               = make_unique_handle<char>(DSPLIB_logicalNOT_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("LogicalNOT constructor", "failed to allocate logicalNOT handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn  = TISP_toDsplibBufParams2D<T>(_in);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<T>(_out);
         DSPLIB_STATUS        status =
             DSPLIB_logicalNOT_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_logicalNOT_init_checkParams")) {
            status = DSPLIB_logicalNOT_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_logicalNOT_init");
         }
      }
   }
}

template <typename T> void *LogicalNOT<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t LogicalNOT<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *LogicalNOT<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t LogicalNOT<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void LogicalNOT<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void LogicalNOT<T>::exec() noexcept
{
   DSPLIB_logicalNOT_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template class LogicalNOT<int8_t>;
template class LogicalNOT<uint8_t>;
template class LogicalNOT<int16_t>;
template class LogicalNOT<uint16_t>;
template class LogicalNOT<int32_t>;
template class LogicalNOT<uint32_t>;
template class LogicalNOT<int64_t>;
template class LogicalNOT<uint64_t>;
template class LogicalNOT<float>;
template class LogicalNOT<double>;

} // namespace DSPLIB
} // namespace TISP
