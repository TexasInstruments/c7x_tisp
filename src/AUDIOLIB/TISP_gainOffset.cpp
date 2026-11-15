// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_gainOffset.hpp>
#include <cmath>

namespace TISP {
namespace AUDIOLIB {
//! [TISP::GainOffset constructor]

template <typename T>
GainOffset<T>::GainOffset(const TISP_BufDescList    *pInputs,
                          const TISP_BufDescList    *pOutputs,
                          const GainOffsetConfig<T> *pConfig,
                          const char                *nodeName,
                          uint32_t                   nodeId,
                          ErrorCtxt                 &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("GainOffset constructor", "build params pointer pConfig is null");
   }
   else if (!std::isfinite(static_cast<double>(pConfig->gain)) ||
            !std::isfinite(static_cast<double>(pConfig->offset))) {
      errorCtx.setInvalidParameterError("GainOffset constructor", "gain and offset must be finite (not NaN or Inf)");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "GainOffset constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "GainOffset constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("GainOffset constructor", "input and output interleave must match");
      }
      else if (_in.pBuf == _out.pBuf) {
         errorCtx.setInvalidParameterError("GainOffset constructor", "in-place operation is not supported");
      }
      else if ((reinterpret_cast<uintptr_t>(_in.pBuf) <
                reinterpret_cast<uintptr_t>(_out.pBuf) + _out.pitchBytes * _out.height) &&
               (reinterpret_cast<uintptr_t>(_out.pBuf) <
                reinterpret_cast<uintptr_t>(_in.pBuf) + _in.pitchBytes * _in.height)) {
         errorCtx.setInvalidParameterError("GainOffset constructor", "input and output buffers must not overlap");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu, height=%zu, isInterleave=%u",
                      _in.width, _in.height, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_gainOffset from AUDIOLIB */
      /***********************************************************************/
      AUDIOLIB_gainOffset_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.pGain     = &pConfig->gain;
      kerInitArgs.pOffset   = &pConfig->offset;

      _handle = make_unique_handle<char>(AUDIOLIB_gainOffset_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("GainOffset constructor", "failed to allocate gainOffset handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/
         AUDIOLIB_STATUS status =
             AUDIOLIB_gainOffset_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for gainOffset to set SE/SA params        */
         /******************************************************************************/
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_gainOffset_init_checkParams")) {
            status = AUDIOLIB_gainOffset_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_gainOffset_init");
         }
      }
   }
}

template <typename T> void *GainOffset<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t GainOffset<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *GainOffset<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t GainOffset<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void GainOffset<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void GainOffset<T>::exec() noexcept
{
   AUDIOLIB_gainOffset_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template class GainOffset<float>;
template class GainOffset<double>;
template class GainOffset<int32_t>;

} // namespace AUDIOLIB
} // namespace TISP
