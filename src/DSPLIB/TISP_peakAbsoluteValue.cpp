// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_peakAbsoluteValue.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename T>
PeakAbsoluteValue<T>::PeakAbsoluteValue(const TISP_BufDescList *pInputs,
                                        const TISP_BufDescList *pOutputs,
                                        const char             *nodeName,
                                        uint32_t                nodeId,
                                        ErrorCtxt              &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "PeakAbsoluteValue constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "PeakAbsoluteValue constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];
   }

   if (errorCtx.isSuccess()) {
      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,isInterleave=%u",
                      _in.width, _in.height, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      DSPLIB_peakAbsoluteValue_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_peakAbsoluteValue_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("PeakAbsoluteValue constructor", "failed to allocate peakAbsoluteValue handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn  = TISP_toDsplibBufParams2D<T>(_in);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<T>(_out);

         DSPLIB_STATUS status =
             DSPLIB_peakAbsoluteValue_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_peakAbsoluteValue_init_checkParams")) {
            status = DSPLIB_peakAbsoluteValue_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_peakAbsoluteValue_init");
         }
      }
   }
}

template <typename T> void *PeakAbsoluteValue<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t PeakAbsoluteValue<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *PeakAbsoluteValue<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t PeakAbsoluteValue<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void PeakAbsoluteValue<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void PeakAbsoluteValue<T>::exec() noexcept
{
   DSPLIB_peakAbsoluteValue_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template class PeakAbsoluteValue<int8_t>;
template class PeakAbsoluteValue<int16_t>;
template class PeakAbsoluteValue<int32_t>;
template class PeakAbsoluteValue<int64_t>;
template class PeakAbsoluteValue<float>;
template class PeakAbsoluteValue<double>;

} // namespace DSPLIB
} // namespace TISP
