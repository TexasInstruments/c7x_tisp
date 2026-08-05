// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_delay.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Delay constructor]

template <typename T>
Delay<T>::Delay(T         *pX,
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
                size_t     delaySize,
                ErrorCtxt &errorCtx)
    : _pX(pX), _pD(pD), _pS(pS), _pY(pY), _historyBufSize(0)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", width, height);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Delay constructor", "input pointer pX is null");
   }
   if (pD == nullptr) {
      errorCtx.setNullPointerError("Delay constructor", "delay pointer pD is null");
   }
   if (pS == nullptr) {
      errorCtx.setNullPointerError("Delay constructor", "scratch memory pointer pS is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Delay constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Delay constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("Delay constructor", "height must be greater than zero");
   }
   if (widthD == 0) {
      errorCtx.setInvalidParameterError("Delay constructor", "widthD must be greater than zero");
   }
   if (heightD == 0) {
      errorCtx.setInvalidParameterError("Delay constructor", "heightD must be greater than zero");
   }
   if (interleave > 1) {
      errorCtx.setInvalidParameterError("Delay constructor", "interleave must be either zero or one");
   }
   if (mode > 1) {
      errorCtx.setInvalidParameterError("Delay constructor", "mode must be either zero or one");
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

      _historyBufSize = heightD * delayBytes;

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
      /* Initialize data structures required by AUDIOLIB_Delay from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_delay_InitArgs kerInitArgs;
      kerInitArgs.funcStyle  = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.interleave = interleave;
      kerInitArgs.mode       = mode;
      kerInitArgs.delaySize  = delaySize;

      _handle = make_unique_handle<char>(AUDIOLIB_delay_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Delay constructor", "failed to allocate delay handle");
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

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_delay_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsDelay, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_delay_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for delay to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet =
                AUDIOLIB_delay_init(_handle.get(), &bufParamsIn, &bufParamsDelay, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_delay_init");
            if (errorCtx.isSuccess()) {
               cleanInternalBuffers();
            }
         }
      }
   }
}

template <typename T> void *Delay<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Delay<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Delay<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Delay<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Delay setAddr]
template <typename T> void Delay<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Delay<T>::exec() noexcept { AUDIOLIB_delay_exec(_handle.get(), _pX, _pD, _pY, _pS); }

template <typename T> void Delay<T>::cleanInternalBuffers() noexcept
{
   if (_pD != nullptr && _historyBufSize > 0) {
      std::memset(_pD, 0, _historyBufSize);
   }
}

template <typename T>
Delay<T>::Delay(T          *pX,
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
                size_t      delaySize,
                const char *nodeName,
                uint32_t    nodeId,
                ErrorCtxt  &errorCtx)
    : Delay(pX,
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
            delaySize,
            errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Delay<float>;
template class Delay<double>;

} // namespace AUDIOLIB
} // namespace TISP
