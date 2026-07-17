// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_muteNCh.hpp>
#include <cstring>
#include <string>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Mutench constructor]

template <typename T>
Mutench<T>::Mutench(T         *pX,
                    T         *pY,
                    size_t     width,
                    size_t     height,
                    size_t     inChannels,
                    size_t     inPitchBytes,
                    size_t     outPitchBytes,
                    size_t     isInterleaved,
                    int32_t    samplingRate,
                    uint32_t  *isMute,
                    float      fadeTime,
                    uint32_t   fadeType,
                    ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numChannels=%zu,samplingRate=%d", inChannels,
                   samplingRate);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Mutench constructor", "input pointer pX is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("Mutench constructor", "output pointer pY is null");
   }
   else if (width == 0) {
      errorCtx.setInvalidParameterError("Mutench constructor", "width must be greater than zero");
   }
   else if (height == 0) {
      errorCtx.setInvalidParameterError("Mutench constructor", "height must be greater than zero");
   }
   else if (inChannels == 0) {
      errorCtx.setInvalidParameterError("Mutench constructor", "inChannels must be greater than zero");
   }
   else if (isInterleaved > 1) {
      errorCtx.setInvalidParameterError("Mutench constructor", "isInterleaved must be either zero or one");
   }
   else if (samplingRate == 0) {
      errorCtx.setInvalidParameterError("Mutench constructor", "samplingRate must be greater than zero");
   }
   else if (isMute == nullptr) {
      errorCtx.setNullPointerError("Mutench constructor", "isMute pointer is null");
   }
   else {
      _width         = width;
      _height        = height;
      _inChannels    = inChannels;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _isInterleaved = isInterleaved;
      _samplingRate  = samplingRate;

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
      /* Initialize data structures required by AUDIOLIB_Mutench from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_muteNCh_InitArgs kerInitArgs;
      kerInitArgs.funcStyle     = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate  = samplingRate;
      kerInitArgs.isInterleaved = isInterleaved;
      kerInitArgs.numChannels   = inChannels;

      _setArgsBuf        = make_unique_handle<char>(sizeof(AUDIOLIB_muteNCh_SetArgs));
      AUDIOLIB_muteNCh_SetArgs *pSetArgs = nullptr;
      if (!_setArgsBuf) {
         errorCtx.setMemoryError("Mutench constructor", "failed to allocate _setArgsBuf");
      }
      else {
         pSetArgs           = reinterpret_cast<AUDIOLIB_muteNCh_SetArgs *>(_setArgsBuf.get());
         pSetArgs->isMute   = isMute;
         pSetArgs->fadeTime = fadeTime;
         pSetArgs->fadeType = fadeType;
      }

      if (errorCtx.isSuccess()) {
         _handle = make_unique_handle<char>(AUDIOLIB_muteNCh_getHandleSize(&kerInitArgs));

         if (!_handle) {
            errorCtx.setMemoryError("Mutench constructor", "failed to allocate muteNCh handle");
         }

         if (errorCtx.isSuccess()) {

            AUDIOLIB_bufParams2D_t bufParamsIn;
            AUDIOLIB_bufParams2D_t bufParamsOut;

            bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsIn.dim_x     = static_cast<uint32_t>(width);
            bufParamsIn.dim_y     = static_cast<uint32_t>(height);
            bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

            bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsOut.dim_x     = static_cast<uint32_t>(width);
            bufParamsOut.dim_y     = static_cast<uint32_t>(height);
            bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
            /******************************************************************************/
            /* Validate parameters before init                                            */
            /******************************************************************************/

            AUDIOLIB_STATUS status =
                AUDIOLIB_muteNCh_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_muteNCh_init_checkParams");
            /******************************************************************************/
            /* Call init function from AUDIOLIB to for muteNCh to set SE/SA params             */
            /******************************************************************************/

            if (errorCtx.isSuccess()) {
               status = AUDIOLIB_muteNCh_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
               errorCtx.remapXLIBError(status, "AUDIOLIB_muteNCh_init");
               if (errorCtx.isSuccess()) {
                  status = AUDIOLIB_muteNCh_set(_handle.get(), pSetArgs);
                  errorCtx.remapXLIBError(status, "AUDIOLIB_muteNCh_set");
               }
            }
         }
      }
   }
}

template <typename T> void *Mutench<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Mutench<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Mutench<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Mutench<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Mutench setAddr]
template <typename T> void Mutench<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Mutench<T>::exec() noexcept
{
   AUDIOLIB_muteNCh_exec(_handle.get(), static_cast<void *>(_pX), static_cast<void *>(_pY));
}

template <typename T>
bool Mutench<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Mutench::setParams", "value pointer is null");
   }
   else if (!_setArgsBuf) {
      errorCtx.setMemoryError("Mutench::setParams", "_setArgsBuf is null");
   }
   else {
      auto                    *pSetArgs = reinterpret_cast<AUDIOLIB_muteNCh_SetArgs *>(_setArgsBuf.get());
      AUDIOLIB_muteNCh_SetArgs setArgs  = {0};
      memcpy(&setArgs, pSetArgs, sizeof(AUDIOLIB_muteNCh_SetArgs));

      const char *paramName = "Unknown";

      switch (static_cast<MutenchParamId>(paramId)) {
      case MutenchParamId::kIsMuteValue: {
         size_t expectedSize = (_inChannels * sizeof(int32_t));
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("Mutench::setParams",
                                              "Value size smaller than expected for kIsMuteValue");
         }
         else {
            memcpy(setArgs.isMute, value, expectedSize);
            paramName = "kIsMuteValue";
         }
         break;
      }
      case MutenchParamId::kFadeTime: {
         if (size < sizeof(setArgs.fadeTime)) {
            errorCtx.setInvalidParameterError("Mutench::setParams", "Value size smaller than expected for kFadeTime");
         }
         else {
            memcpy(&setArgs.fadeTime, value, sizeof(setArgs.fadeTime));
            paramName = "kFadeTime";
         }
         break;
      }
      case MutenchParamId::kFadeType: {
         if (size < sizeof(setArgs.fadeType)) {
            errorCtx.setInvalidParameterError("Mutench::setParams", "Value size smaller than expected for kFadeType");
         }
         else {
            memcpy(&setArgs.fadeType, value, sizeof(setArgs.fadeType));
            paramName = "kFadeType";
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Mutench::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_STATUS status = AUDIOLIB_muteNCh_set(_handle.get(), &setArgs);
         std::string     msg    = std::string("AUDIOLIB_muteNCh_set with ") + paramName;
         (void) errorCtx.remapXLIBError(status, msg.c_str());
         if (errorCtx.isSuccess()) {
            memcpy(pSetArgs, &setArgs, sizeof(AUDIOLIB_muteNCh_SetArgs));
         }
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool Mutench<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Mutench::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("Mutench::getParams", "size pointer is null");
   }
   else if (!_setArgsBuf) {
      errorCtx.setMemoryError("Mutench::getParams", "_setArgsBuf is null");
   }
   else {
      auto *pSetArgs = reinterpret_cast<AUDIOLIB_muteNCh_SetArgs *>(_setArgsBuf.get());

      switch (static_cast<MutenchParamId>(paramId)) {
      case MutenchParamId::kIsMuteValue: {
         size_t expectedSize = (_inChannels * sizeof(int32_t));
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("Mutench::getParams",
                                              "Value size smaller than expected for kIsMuteValue");
         }
         else {
            memcpy(value, pSetArgs->isMute, expectedSize);
         }
         *size = expectedSize;
         break;
      }
      case MutenchParamId::kFadeTime: {
         if (*size < sizeof(pSetArgs->fadeTime)) {
            errorCtx.setInvalidParameterError("Mutench::getParams", "Value size smaller than expected for kFadeTime");
         }
         else {
            memcpy(value, &pSetArgs->fadeTime, sizeof(pSetArgs->fadeTime));
         }
         *size = sizeof(pSetArgs->fadeTime);
         break;
      }
      case MutenchParamId::kFadeType: {
         if (*size < sizeof(pSetArgs->fadeType)) {
            errorCtx.setInvalidParameterError("Mutench::getParams", "Value size smaller than expected for kFadeType");
         }
         else {
            memcpy(value, &pSetArgs->fadeType, sizeof(pSetArgs->fadeType));
         }
         *size = sizeof(pSetArgs->fadeType);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Mutench::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
Mutench<T>::Mutench(T          *pX,
                    T          *pY,
                    size_t      width,
                    size_t      height,
                    size_t      inChannels,
                    size_t      inPitchBytes,
                    size_t      outPitchBytes,
                    size_t      isInterleaved,
                    int32_t     samplingRate,
                    uint32_t   *isMute,
                    float       fadeTime,
                    uint32_t    fadeType,
                    const char *nodeName,
                    uint32_t    nodeId,
                    ErrorCtxt  &errorCtx)
    : Mutench(pX,
              pY,
              width,
              height,
              inChannels,
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

template class Mutench<float>;

} // namespace AUDIOLIB
} // namespace TISP
