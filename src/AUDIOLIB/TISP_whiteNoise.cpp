// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_whiteNoise.hpp>

namespace TISP {
namespace AUDIOLIB {
//! [TISP::Whitenoise constructor]

template <typename T>
Whitenoise<T>::Whitenoise(const TISP_BufDescList *pInputs,
                          const TISP_BufDescList *pOutputs,
                          const WhitenoiseConfig *pConfig,
                          const char             *nodeName,
                          uint32_t                nodeId,
                          ErrorCtxt              &errorCtx)
    : _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("Whitenoise constructor", "build params pointer pConfig is null");
   }
   else if (pConfig->range <= 0.0f) {
      errorCtx.setInvalidParameterError("Whitenoise constructor", "range must be greater than zero");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): no input, one output
   if (TISP_checkBufDescList<T>(pInputs, 0U, 0U, "Whitenoise constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Whitenoise constructor (outputs)", errorCtx)) {
      _out = pOutputs->pDesc[0];

      if (_out.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("Whitenoise constructor", "interleaved output is not supported");
      }
   }

   if (errorCtx.isSuccess()) {
      // Initialize _params and bufParams with the node parameters (source node: no input buffer)
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu, height=%zu, range=%.2f, seed=%u",
                      _out.width, _out.height, static_cast<double>(pConfig->range), pConfig->seed);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, nullptr, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_whiteNoiseGenerator from AUDIOLIB */
      /***********************************************************************/
      AUDIOLIB_whiteNoiseGenerator_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.seed      = pConfig->seed;
      kerInitArgs.range     = pConfig->range;

      _handle = make_unique_handle<char>(AUDIOLIB_whiteNoiseGenerator_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("Whitenoise constructor", "failed to allocate whiteNoiseGenerator handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/
         AUDIOLIB_STATUS status =
             AUDIOLIB_whiteNoiseGenerator_init_checkParams(_handle.get(), &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for whiteNoiseGenerator to set SA params  */
         /******************************************************************************/
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_whiteNoiseGenerator_init_checkParams")) {
            status = AUDIOLIB_whiteNoiseGenerator_init(_handle.get(), &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_whiteNoiseGenerator_init");
         }
      }
   }
}

template <typename T> void *Whitenoise<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Whitenoise<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Whitenoise setAddr]
template <typename T> void Whitenoise<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void Whitenoise<T>::exec() noexcept
{
   AUDIOLIB_whiteNoiseGenerator_exec(_handle.get(), _out.pBuf);
}

template class Whitenoise<float>;

} // namespace AUDIOLIB
} // namespace TISP
