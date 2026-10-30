// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_deEmphasis.hpp>

namespace TISP {
namespace AUDIOLIB {
//! [TISP::DeEmphasis constructor]

template <typename T>
DeEmphasis<T>::DeEmphasis(const TISP_BufDescList *pInputs,
                          const TISP_BufDescList *pOutputs,
                          const DeEmphasisConfig *pConfig,
                          const char             *nodeName,
                          uint32_t                nodeId,
                          ErrorCtxt              &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("DeEmphasis constructor", "build params pointer pConfig is null");
   }
   else if ((pConfig->alpha < 0.0f) || (pConfig->alpha > 1.0f)) {
      errorCtx.setInvalidParameterError("DeEmphasis constructor", "alpha must be in the range [0, 1]");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "DeEmphasis constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "DeEmphasis constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("DeEmphasis constructor", "input and output interleave must match");
      }
      else if (_in.pBuf == _out.pBuf) {
         errorCtx.setInvalidParameterError("DeEmphasis constructor", "in-place operation is not supported");
      }
      // Interleaved: the kernel reads the filter state back as whole vectors of 8 channels from the last output row
      else if ((_out.isInterleave == 1U) && (_out.pitchBytes < (((_out.width + 7U) / 8U) * 8U * sizeof(T)))) {
         errorCtx.setInvalidParameterError("DeEmphasis constructor",
                                           "interleaved output pitch must cover the channel count rounded up to 8");
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
      /* Initialize data structures required by AUDIOLIB_deEmphasis from AUDIOLIB */
      /***********************************************************************/
      AUDIOLIB_deEmphasis_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = _in.isInterleave;
      kerInitArgs.numChannels  = static_cast<uint32_t>(numChannels);
      kerInitArgs.alpha        = pConfig->alpha;

      _handle = make_unique_handle<char>(AUDIOLIB_deEmphasis_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("DeEmphasis constructor", "failed to allocate deEmphasis handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/
         AUDIOLIB_STATUS status =
             AUDIOLIB_deEmphasis_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for deEmphasis to set SE/SA params    */
         /******************************************************************************/
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_deEmphasis_init_checkParams")) {
            status = AUDIOLIB_deEmphasis_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_deEmphasis_init");
         }
      }
   }
}

template <typename T> void *DeEmphasis<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *DeEmphasis<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t DeEmphasis<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t DeEmphasis<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::DeEmphasis setAddr]
template <typename T> void DeEmphasis<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void DeEmphasis<T>::exec() noexcept
{
   AUDIOLIB_deEmphasis_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template class DeEmphasis<float>;

} // namespace AUDIOLIB
} // namespace TISP
