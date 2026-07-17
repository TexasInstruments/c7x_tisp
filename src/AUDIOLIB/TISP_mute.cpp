// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_mute.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Mute constructor]

template <typename T>
Mute<T>::Mute(T         *pX,
              T         *pY,
              size_t     width,
              size_t     height,
              size_t     inPitchBytes,
              size_t     outPitchBytes,
              size_t     isInterleaved,
              int32_t    samplingRate,
              uint32_t   isMute,
              float      fadeTime,
              uint32_t   fadeType,
              ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,samplingRate=%d", width,
                   height, samplingRate);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Mute constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Mute constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Mute constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("Mute constructor", "height must be greater than zero");
   }
   if (isInterleaved > 1) {
      errorCtx.setInvalidParameterError("Mute constructor", "isInterleaved must be either zero or one");
   }
   if (samplingRate == 0) {
      errorCtx.setInvalidParameterError("Mute constructor", "samplingRate must be greater than zero");
   }
   if (isMute > 1) {
      errorCtx.setInvalidParameterError("Mute constructor", "isMute must be either zero or one");
   }
   if (fadeTime < 0) {
      errorCtx.setInvalidParameterError("Mute constructor", "fadeTime must be greater than or equal to zero");
   }
   if (fadeType > 2) {
      errorCtx.setInvalidParameterError("Mute constructor", "fadeType must be either zero or one or two");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width         = width;
      _height        = height;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _isInterleaved = isInterleaved;
      _samplingRate  = samplingRate;
      _isMute        = isMute;
      _fadeTime      = fadeTime;
      _fadeType      = fadeType;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleaved;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleaved;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Mute from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_mute_InitArgs kerInitArgs;
      kerInitArgs.funcStyle     = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate  = samplingRate;
      kerInitArgs.isInterleaved = isInterleaved;

      AUDIOLIB_mute_SetArgs kerSetArgs;
      kerSetArgs.isMute   = isMute;
      kerSetArgs.fadeTime = fadeTime;
      kerSetArgs.fadeType = fadeType;

      _handle = make_unique_handle<char>(AUDIOLIB_mute_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Mute constructor", "failed to allocate mute handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.stride_y  = static_cast<uint32_t>(inPitchBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<uint32_t>(outPitchBytes);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_mute_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_mute_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for mute to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_mute_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_mute_init");
            if (errorCtx.isSuccess()) {
               AUDIOLIB_STATUS setRet = AUDIOLIB_mute_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(setRet, "AUDIOLIB_mute_set");
            }
         }
      }
   }
}

template <typename T>
Mute<T>::Mute(T          *pX,
              T          *pY,
              size_t      width,
              size_t      height,
              size_t      inPitchBytes,
              size_t      outPitchBytes,
              size_t      isInterleaved,
              int32_t     samplingRate,
              uint32_t    isMute,
              float       fadeTime,
              uint32_t    fadeType,
              const char *nodeName,
              uint32_t    nodeId,
              ErrorCtxt  &errorCtx)
    : Mute(pX,
           pY,
           width,
           height,
           inPitchBytes,
           outPitchBytes,
           isInterleaved,
           samplingRate,
           isMute,
           fadeTime,
           fadeType,
           errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Mute<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Mute<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Mute<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Mute<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Mute setAddr]
template <typename T> void Mute<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Mute<T>::exec() noexcept
{
   AUDIOLIB_mute_exec(_handle.get(), static_cast<void *>(_pX), static_cast<void *>(_pY));
}

template class Mute<float>;

} // namespace AUDIOLIB
} // namespace TISP
