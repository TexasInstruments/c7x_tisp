// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_delayNChannel.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::DelayNChannel constructor]

template <typename T>
DelayNChannel<T>::DelayNChannel(const TISP_BufDescList         *pInputs,
                                const TISP_BufDescList         *pOutputs,
                                const TISP_BufDescList         *pStates,
                                void                           *pScratch,
                                size_t                          scratchBytes,
                                const DelayNChannelConfig *pConfig,
                                const char                     *nodeName,
                                uint32_t                        nodeId,
                                ErrorCtxt                      &errorCtx)
    : _in{}, _out{}, _state{}, _scratch(static_cast<T *>(pScratch)), _mode(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("DelayNChannel constructor", "build params pointer pConfig is null");
   }
   else if (pConfig->pDelaySize == nullptr) {
      errorCtx.setNullPointerError("DelayNChannel constructor", "delay size pointer pConfig->pDelaySize is null");
   }
   else if (pScratch == nullptr) {
      errorCtx.setNullPointerError("DelayNChannel constructor", "scratch pointer pScratch is null");
   }
   else {
      /* Nothing to do here */
   }

   size_t numChannels = 0U;

   // Buffer checks (skipped once an error is set): one input, one output, and one delay line
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "DelayNChannel constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "DelayNChannel constructor (outputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pStates, 1U, 1U, "DelayNChannel constructor (states)", errorCtx)) {
      _in    = pInputs->pDesc[0];
      _out   = pOutputs->pDesc[0];
      _state = pStates->pDesc[0];

      // One delay per channel: channels are columns when interleaved, rows otherwise
      numChannels = (_in.isInterleave == 1U) ? _in.width : _in.height;

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("DelayNChannel constructor", "input and output interleave must match");
      }
      else if (numChannels > MAX_NUM_CHANNELS) {
         errorCtx.setInvalidParameterError("DelayNChannel constructor",
                                           "number of channels must not exceed MAX_NUM_CHANNELS");
      }
      else if (_state.height < numChannels) {
         errorCtx.setInvalidParameterError("DelayNChannel constructor", "delay state must have one row per channel");
      }
      else if (scratchBytes < (_in.width * _in.height * sizeof(T))) {
         errorCtx.setInvalidParameterError("DelayNChannel constructor",
                                           "scratchBytes is smaller than channels x samples");
      }
      else if (pConfig->mode > 1U) {
         errorCtx.setInvalidParameterError("DelayNChannel constructor", "mode must be either zero or one");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _mode = pConfig->mode;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,widthD=%zu,heightD=%zu",
                      _in.width, _in.height, _state.width, _state.height);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Delaynchannel from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_delayNChannel_InitArgs kerInitArgs;
      kerInitArgs.funcStyle  = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.interleave = _in.isInterleave;
      kerInitArgs.mode       = _mode;
      for (size_t ch = 0; ch < numChannels; ch++) {
         kerInitArgs.delaySize[ch] = pConfig->pDelaySize[ch]; // NOLINT(MISRA.PTR.ARITH)
      }

      _handle = make_unique_handle<char>(AUDIOLIB_delayNChannel_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("DelayNChannel constructor", "failed to allocate delayNChannel handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn    = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsDelay = TISP_toAudiolibBufParams2D<T>(_state);
         AUDIOLIB_bufParams2D_t bufParamsOut   = TISP_toAudiolibBufParams2D<T>(_out);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status = AUDIOLIB_delayNChannel_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsDelay,
                                                                          &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for delayNChannel to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "AUDIOLIB_delayNChannel_init_checkParams")) {
            status =
                AUDIOLIB_delayNChannel_init(_handle.get(), &bufParamsIn, &bufParamsDelay, &bufParamsOut, &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "AUDIOLIB_delayNChannel_init")) {
               cleanInternalBuffers();
            }
         }
      }
   }
}

template <typename T> void *DelayNChannel<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *DelayNChannel<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t DelayNChannel<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t DelayNChannel<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::DelayNChannel setAddr]
template <typename T> void DelayNChannel<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void DelayNChannel<T>::exec() noexcept
{
   AUDIOLIB_delayNChannel_exec(_handle.get(), _in.pBuf, _state.pBuf, _out.pBuf, _scratch);
}

template <typename T> void DelayNChannel<T>::cleanInternalBuffers() noexcept
{
   if (_state.pBuf != nullptr && _state.height > 0) {
      if (_mode == 0) {
         /* Linear delay: row i is only indexed up to its own delaySize[i], and the state width is the
          * maximum delay across channels, so it bounds every row's live region. The remainder of
          * the row up to the state pitch is padding the kernel never touches. */
         for (size_t row = 0; row < _state.height; row++) {
            uint8_t *pRow = static_cast<uint8_t *>(_state.pBuf) + (row * _state.pitchBytes); // NOLINT(MISRA.PTR.ARITH)
            std::memset(pRow, 0, _state.width * sizeof(T));
         }
      }
      else {
         /* Circular delay: AUDIOLIB_delayNChannel_init derives delayBuffSize from stride_y, so the
          * per-channel readIdx[]/writeIdx[] wrap over the whole state pitch. The entire
          * row is live circular buffer - clearing only the state width would leave stale samples behind. */
         std::memset(_state.pBuf, 0, _state.height * _state.pitchBytes);
      }
   }
}

template class DelayNChannel<float>;
template class DelayNChannel<double>;

} // namespace AUDIOLIB
} // namespace TISP
