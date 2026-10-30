// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_floor.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {
template <typename T>
Floor<T>::Floor(const TISP_BufDescList *pInputs,
                const TISP_BufDescList *pOutputs,
                const char             *nodeName,
                uint32_t                nodeId,
                ErrorCtxt              &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Floor constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Floor constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_out.isInterleave != _in.isInterleave) {
         errorCtx.setInvalidParameterError("Floor constructor", "output 0 layout must match input 0");
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

      AUDIOLIB_floor_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;
      _handle               = make_unique_handle<char>(AUDIOLIB_floor_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("Floor constructor", "failed to allocate floor handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);
         AUDIOLIB_STATUS        status =
             AUDIOLIB_floor_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_floor_init_checkParams")) {
            status = AUDIOLIB_floor_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_floor_init");
         }
      }
   }
}

template <typename T> void *Floor<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t Floor<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *Floor<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Floor<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Floor<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void Floor<T>::exec() noexcept { AUDIOLIB_floor_exec(_handle.get(), _in.pBuf, _out.pBuf); }

template class Floor<float>;
template class Floor<double>;

} // namespace AUDIOLIB
} // namespace TISP
