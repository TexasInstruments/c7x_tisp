// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <TISP_concat.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
Concat<T>::Concat(const TISP_BufDescList *pInputs,
                  const TISP_BufDescList *pOutputs,
                  const char             *nodeName,
                  uint32_t                nodeId,
                  ErrorCtxt              &errorCtx)
    : _inputs{}, _out{}, _numInputs(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   // Buffer checks: 1 to kMaxSrcAddr inputs, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, static_cast<size_t>(kMaxSrcAddr), "Concat constructor (inputs)",
                                errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Concat constructor (outputs)", errorCtx)) {
      _numInputs = pInputs->numBufs;
      _out       = pOutputs->pDesc[0];

      for (size_t i = 0U; (i < _numInputs) && errorCtx.isSuccess(); i++) {
         if (pInputs->pDesc[i].isInterleave != _out.isInterleave) { // NOLINT(MISRA.PTR.ARITH)
            errorCtx.setInvalidParameterError("Concat constructor", "input and output interleave must match");
         }
      }
   }

   if (errorCtx.isSuccess()) {
      std::array<uint32_t, kMaxSrcAddr>               inChannels{};
      std::array<AUDIOLIB_bufParams2D_t, kMaxSrcAddr> bufParamsIn{};
      uint32_t                                        totalInChannels = 0U;

      for (size_t i = 0U; i < _numInputs; i++) {
         const TISP_BufDesc2D &in = pInputs->pDesc[i]; // NOLINT(MISRA.PTR.ARITH)
         _inputs[i]               = in.pBuf;
         inChannels[i]            = static_cast<uint32_t>((_out.isInterleave == 1U) ? in.width : in.height);
         totalInChannels += inChannels[i];
         bufParamsIn[i] = TISP_toAudiolibBufParams2D<T>(in);
      }

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "numInputs=%zu,totalInChannels=%u,inSamples=%zu", _numInputs, totalInChannels,
                      (_out.isInterleave == 1U) ? _out.height : _out.width);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_concat from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_concat_InitArgs initArgs;
      initArgs.funcStyle       = AUDIOLIB_FUNCTION_OPTIMIZED;
      initArgs.inChannels      = inChannels.data();
      initArgs.numInputs       = static_cast<uint32_t>(_numInputs);
      initArgs.totalInChannels = totalInChannels;
      initArgs.isInterleave    = _out.isInterleave;

      _handle = make_unique_handle<char>(AUDIOLIB_concat_getHandleSize(&initArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Concat constructor", "failed to allocate concat handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status =
             AUDIOLIB_concat_init_checkParams(_handle.get(), bufParamsIn.data(), &bufParamsOut, &initArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for concat to set SE/SA params           */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "AUDIOLIB_concat_init_checkParams")) {
            status = AUDIOLIB_concat_init(_handle.get(), bufParamsIn.data(), &bufParamsOut, &initArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_concat_init");
         }
      }
   }
}

template <typename T> void *Concat<T>::getInputAddr(size_t index) const noexcept
{
   return (index < _numInputs) ? _inputs[index] : nullptr;
}

template <typename T> void *Concat<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Concat<T>::getNumInputs() const noexcept { return _numInputs; }

template <typename T> size_t Concat<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Concat<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      for (size_t i = 0U; i < _numInputs; i++) {
         _inputs[i] = addrs.src[i];
      }
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void Concat<T>::exec() noexcept
{
   AUDIOLIB_concat_exec(_handle.get(), _inputs.data(), _out.pBuf);
}

template class Concat<float>;
template class Concat<double>;

} // namespace AUDIOLIB
} // namespace TISP
