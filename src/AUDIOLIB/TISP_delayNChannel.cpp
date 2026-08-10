// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_delayNChannel.hpp>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::DelayNChannel constructor]

template <typename T>
DelayNChannel<T>::DelayNChannel(T         *pX,
                                T         *pD,
                                T         *pS,
                                T         *pY,
                                size_t     width,
                                size_t     height,
                                size_t     widthD,
                                size_t     heightD,
                                size_t     inPitchBytes,
                                size_t     delayBytes,
                                size_t     outPitchBytes,
                                size_t     interleave,
                                size_t     mode,
                                uint32_t  *pDelaySize,
                                ErrorCtxt &errorCtx)
    : _pX(pX), _pD(pD), _pS(pS), _pY(pY)
{
   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "width=%zu,height=%zu,widthD=%zu,heightD=%zu", width, height, widthD, heightD);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("DelayNChannel constructor", "input pointer pX is null");
   }
   if (pD == nullptr) {
      errorCtx.setNullPointerError("DelayNChannel constructor", "delay pointer pD is null");
   }
   if (pS == nullptr) {
      errorCtx.setNullPointerError("DelayNChannel constructor", "scratch memory pointer pS is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("DelayNChannel constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("DelayNChannel constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("DelayNChannel constructor", "height must be greater than zero");
   }
   if (widthD == 0) {
      errorCtx.setInvalidParameterError("DelayNChannel constructor", "widthD must be greater than zero");
   }
   if (heightD == 0) {
      errorCtx.setInvalidParameterError("DelayNChannel constructor", "heightD must be greater than zero");
   }
   if (heightD > MAX_NUM_CHANNELS) {
      errorCtx.setInvalidParameterError("DelayNChannel constructor", "heightD must not exceed MAX_NUM_CHANNELS");
   }
   if (pDelaySize == nullptr) {
      errorCtx.setInvalidParameterError("DelayNChannel constructor", "delay size pointer pDelaySize is null");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width         = width;
      _height        = height;
      _widthD        = widthD;
      _heightD       = heightD;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _delayBytes    = delayBytes;
      _interleave    = interleave;
      _mode          = mode;

      // Initialize bufParams with node input and output buffer parameters. The delay (pD)
      // and scratch (pS) buffers are internal/owned state, not exposed via getInputAddr(),
      // so only the primary data buffer pX/pY is represented here.
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = interleave;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = interleave;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Delaynchannel from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_delayNChannel_InitArgs kerInitArgs;
      kerInitArgs.funcStyle  = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.interleave = interleave;
      kerInitArgs.mode       = mode;
      uint32_t *pDelay       = pDelaySize;
      uint32_t  ch;
      for (ch = 0; ch < heightD; ch++) {
         kerInitArgs.delaySize[ch] = pDelay[ch];
      }

      _handle = make_unique_handle<char>(AUDIOLIB_delayNChannel_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("DelayNChannel constructor", "failed to allocate delayNChannel handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;
         AUDIOLIB_bufParams2D_t bufParamsDelay;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsDelay.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsDelay.dim_x     = static_cast<uint32_t>(widthD);
         bufParamsDelay.dim_y     = static_cast<uint32_t>(heightD);
         bufParamsDelay.stride_y  = static_cast<int32_t>(delayBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet = AUDIOLIB_delayNChannel_init_checkParams(
             _handle.get(), &bufParamsIn, &bufParamsDelay, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_delayNChannel_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for delayNChannel to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet =
                AUDIOLIB_delayNChannel_init(_handle.get(), &bufParamsIn, &bufParamsDelay, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_delayNChannel_init");
         }
      }
   }
}

template <typename T>
DelayNChannel<T>::DelayNChannel(T          *pX,
                                T          *pD,
                                T          *pS,
                                T          *pY,
                                size_t      width,
                                size_t      height,
                                size_t      widthD,
                                size_t      heightD,
                                size_t      inPitchBytes,
                                size_t      delayBytes,
                                size_t      outPitchBytes,
                                size_t      interleave,
                                size_t      mode,
                                uint32_t   *pDelaySize,
                                const char *nodeName,
                                uint32_t    nodeId,
                                ErrorCtxt  &errorCtx)
    : DelayNChannel(pX,
                    pD,
                    pS,
                    pY,
                    width,
                    height,
                    widthD,
                    heightD,
                    inPitchBytes,
                    delayBytes,
                    outPitchBytes,
                    interleave,
                    mode,
                    pDelaySize,
                    errorCtx

      )
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *DelayNChannel<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *DelayNChannel<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t DelayNChannel<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t DelayNChannel<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::DelayNChannel setAddr]
template <typename T> void DelayNChannel<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<T *>(addr.src[0]);
   }
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<T *>(addr.src[0]);
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void DelayNChannel<T>::exec() noexcept
{
   AUDIOLIB_delayNChannel_exec(_handle.get(), _pX, _pD, _pY, _pS);
}

template class DelayNChannel<float>;
template class DelayNChannel<double>;

} // namespace AUDIOLIB
} // namespace TISP
