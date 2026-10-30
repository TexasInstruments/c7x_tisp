// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <TISP_split.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
Split<T>::Split(const TISP_BufDescList *pInputs,
                const TISP_BufDescList *pOutputs,
                const char             *nodeName,
                uint32_t                nodeId,
                ErrorCtxt              &errorCtx)
    : _in{}, _outputs{}, _numOutputs(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   // Buffer checks: one input, 1 to kMaxDstAddr outputs
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Split constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, static_cast<size_t>(kMaxDstAddr), "Split constructor (outputs)",
                                errorCtx)) {
      _in         = pInputs->pDesc[0];
      _numOutputs = pOutputs->numBufs;

      for (size_t i = 0U; (i < _numOutputs) && errorCtx.isSuccess(); i++) {
         if (pOutputs->pDesc[i].isInterleave != _in.isInterleave) { // NOLINT(MISRA.PTR.ARITH)
            errorCtx.setInvalidParameterError("Split constructor", "input and output interleave must match");
         }
      }
   }

   if (errorCtx.isSuccess()) {
      std::array<uint32_t, kMaxDstAddr>               outChannels{};
      std::array<AUDIOLIB_bufParams2D_t, kMaxDstAddr> bufParamsOut{};
      uint32_t                                        totalInputChannels = 0U;

      for (size_t i = 0U; i < _numOutputs; i++) {
         const TISP_BufDesc2D &out = pOutputs->pDesc[i]; // NOLINT(MISRA.PTR.ARITH)
         _outputs[i]               = out.pBuf;
         outChannels[i]            = static_cast<uint32_t>((_in.isInterleave == 1U) ? out.width : out.height);
         totalInputChannels += outChannels[i];
         bufParamsOut[i] = TISP_toAudiolibBufParams2D<T>(out);
      }

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "numOutputs=%zu,totalInputChannels=%u,inSamples=%zu", _numOutputs, totalInputChannels,
                      (_in.isInterleave == 1U) ? _in.height : _in.width);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_split from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_split_InitArgs initArgs;
      initArgs.funcStyle          = AUDIOLIB_FUNCTION_OPTIMIZED;
      initArgs.outChannels        = outChannels.data();
      initArgs.numOutputs         = static_cast<uint32_t>(_numOutputs);
      initArgs.totalInputChannels = totalInputChannels;
      initArgs.isInputInterleave  = _in.isInterleave;

      _handle = make_unique_handle<char>(AUDIOLIB_split_getHandleSize(&initArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Split constructor", "failed to allocate split handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn = TISP_toAudiolibBufParams2D<T>(_in);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status =
             AUDIOLIB_split_init_checkParams(_handle.get(), &bufParamsIn, bufParamsOut.data(), &initArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for split to set SE/SA params            */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "AUDIOLIB_split_init_checkParams")) {
            status = AUDIOLIB_split_init(_handle.get(), &bufParamsIn, bufParamsOut.data(), &initArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_split_init");
         }
      }
   }
}

template <typename T> void *Split<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *Split<T>::getOutputAddr(size_t index) const noexcept
{
   return (index < _numOutputs) ? _outputs[index] : nullptr;
}

template <typename T> size_t Split<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Split<T>::getNumOutputs() const noexcept { return _numOutputs; }

template <typename T> void Split<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      for (size_t i = 0U; i < _numOutputs; i++) {
         _outputs[i] = addrs.dst[i];
      }
   }
}

template <typename T> void Split<T>::exec() noexcept { AUDIOLIB_split_exec(_handle.get(), _in.pBuf, _outputs.data()); }

template class Split<float>;
template class Split<double>;

} // namespace AUDIOLIB
} // namespace TISP
