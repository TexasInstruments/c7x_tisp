// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_preEmphasis.hpp>

namespace TISP {
namespace AUDIOLIB {
//! [TISP::PreEmphasis constructor]

template <typename T>
PreEmphasis<T>::PreEmphasis(const TISP_BufDescList  *pInputs,
                            const TISP_BufDescList  *pOutputs,
                            const PreEmphasisConfig *pConfig,
                            const char              *nodeName,
                            uint32_t                 nodeId,
                            ErrorCtxt               &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("PreEmphasis constructor", "build params pointer pConfig is null");
   }
   else if ((pConfig->alpha < 0.0f) || (pConfig->alpha > 1.0f)) {
      errorCtx.setInvalidParameterError("PreEmphasis constructor", "alpha must be in the range [0, 1]");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "PreEmphasis constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "PreEmphasis constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("PreEmphasis constructor", "input and output interleave must match");
      }
      else if (_in.pBuf == _out.pBuf) {
         errorCtx.setInvalidParameterError("PreEmphasis constructor", "in-place operation is not supported");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // One filter state per channel: channels are columns when interleaved, rows otherwise
      const size_t numChannels = (_in.isInterleave == 1U) ? _in.width : _in.height;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "width=%zu, height=%zu, numChannels=%zu, isInterleave=%u, alpha=%.4f", _in.width, _in.height,
                      numChannels, static_cast<uint32_t>(_in.isInterleave), static_cast<double>(pConfig->alpha));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_preEmphasis from AUDIOLIB */
      /***********************************************************************/
      AUDIOLIB_preEmphasis_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = _in.isInterleave;
      kerInitArgs.alpha        = pConfig->alpha;

      _handle = make_unique_handle<char>(AUDIOLIB_preEmphasis_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("PreEmphasis constructor", "failed to allocate preEmphasis handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/
         AUDIOLIB_STATUS status =
             AUDIOLIB_preEmphasis_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for preEmphasis to set SE/SA params    */
         /******************************************************************************/
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_preEmphasis_init_checkParams")) {
            status = AUDIOLIB_preEmphasis_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_preEmphasis_init");
         }
      }
   }
}

template <typename T> void *PreEmphasis<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *PreEmphasis<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t PreEmphasis<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t PreEmphasis<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::PreEmphasis setAddr]
template <typename T> void PreEmphasis<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _in.pBuf = addr.src[0];
   }
   if (mode == kAddrModeOut) {
      _out.pBuf = addr.dst[0];
   }
   if (mode == kAddrModeInOut) {
      _in.pBuf  = addr.src[0];
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void PreEmphasis<T>::exec() noexcept
{
   AUDIOLIB_preEmphasis_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template class PreEmphasis<float>;

} // namespace AUDIOLIB
} // namespace TISP
