// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_nonZeroCheck.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename T>
NonZeroCheck<T>::NonZeroCheck(const TISP_BufDescList   *pInputs,
                              const TISP_BufDescList   *pOutputs,
                              const NonZeroCheckConfig *pConfig,
                              const char               *nodeName,
                              uint32_t                  nodeId,
                              ErrorCtxt                &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("NonZeroCheck constructor", "build params pointer pConfig is null");
   }
   else {
      /* Nothing to do here */
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "NonZeroCheck constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "NonZeroCheck constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (pConfig->opType > 1U) {
         errorCtx.setInvalidParameterError("NonZeroCheck constructor", "opType is out of range");
      }
      else if (pConfig->axisType > 1U) {
         errorCtx.setInvalidParameterError("NonZeroCheck constructor", "axisType is out of range");
      }
      else if (_out.height != 1U) {
         errorCtx.setInvalidParameterError("NonZeroCheck constructor", "output 0 height must be 1");
      }
      else if (_out.width != ((pConfig->axisType == 0U) ? _in.width : _in.height)) {
         errorCtx.setInvalidParameterError("NonZeroCheck constructor",
                                           "output 0 width must be the input width (ROW) or height (COLUMN)");
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

      DSPLIB_nonZeroCheck_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.opType    = pConfig->opType;
      kerInitArgs.axisType  = pConfig->axisType;

      _handle = make_unique_handle<char>(DSPLIB_nonZeroCheck_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("NonZeroCheck constructor", "failed to allocate nonZeroCheck handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn = TISP_toDsplibBufParams2D<T>(_in);
         DSPLIB_bufParams1D_t bufParamsOut;
         bufParamsOut.data_type = static_cast<uint32_t>(DSPLIB_element_dType<T>::value);
         bufParamsOut.dim_x     = static_cast<uint32_t>(_out.width);

         DSPLIB_STATUS status =
             DSPLIB_nonZeroCheck_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_nonZeroCheck_init_checkParams")) {
            status = DSPLIB_nonZeroCheck_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_nonZeroCheck_init");
         }
      }
   }
}

template <typename T> void *NonZeroCheck<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t NonZeroCheck<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *NonZeroCheck<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t NonZeroCheck<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void NonZeroCheck<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void NonZeroCheck<T>::exec() noexcept
{
   DSPLIB_nonZeroCheck_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template class NonZeroCheck<int8_t>;
template class NonZeroCheck<uint8_t>;
template class NonZeroCheck<int16_t>;
template class NonZeroCheck<uint16_t>;
template class NonZeroCheck<int32_t>;
template class NonZeroCheck<uint32_t>;
template class NonZeroCheck<int64_t>;
template class NonZeroCheck<uint64_t>;
template class NonZeroCheck<float>;
template class NonZeroCheck<double>;

} // namespace DSPLIB
} // namespace TISP
