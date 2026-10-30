// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_gainSmoothing.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::GainSmoothing constructor]

template <typename T>
GainSmoothing<T>::GainSmoothing(const TISP_BufDescList              *pInputs,
                                const TISP_BufDescList              *pOutputs,
                                const GainSmoothingConfig           *pConfig,
                                const GainSmoothingTunableParams<T> *pDefaultTunableParams,
                                const char                          *nodeName,
                                uint32_t                             nodeId,
                                ErrorCtxt                           &errorCtx)
    : _in{}, _out{}, _config{}, _tunableParams{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("GainSmoothing constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("GainSmoothing constructor", "default params pointer pDefaultTunableParams is null");
   }
   else {
      _config        = *pConfig;
      _tunableParams = *pDefaultTunableParams;
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "GainSmoothing constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "GainSmoothing constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_out.isInterleave != _in.isInterleave) {
         errorCtx.setInvalidParameterError("GainSmoothing constructor", "output 0 layout must match input 0");
      }
      else if ((_config.samplingRate < 1.0f) || (_config.samplingRate > 192000.0f)) {
         errorCtx.setInvalidParameterError("GainSmoothing constructor", "samplingRate is out of range");
      }
      else if (_tunableParams.bypassMode > 1U) {
         errorCtx.setInvalidParameterError("GainSmoothing constructor", "bypassMode is out of range");
      }
      else if ((_tunableParams.currentGain < 0.0f) || (_tunableParams.currentGain > 1000.0f)) {
         errorCtx.setInvalidParameterError("GainSmoothing constructor", "currentGain is out of range");
      }
      else if ((_tunableParams.targetGain < 0.0f) || (_tunableParams.targetGain > 1000.0f)) {
         errorCtx.setInvalidParameterError("GainSmoothing constructor", "targetGain is out of range");
      }
      else if ((_tunableParams.fadeTime < 0.0f) || (_tunableParams.fadeTime > 10000.0f)) {
         errorCtx.setInvalidParameterError("GainSmoothing constructor", "fadeTime is out of range");
      }
      else if (_tunableParams.fadeType > 1U) {
         errorCtx.setInvalidParameterError("GainSmoothing constructor", "fadeType is out of range");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,isInterleave=%u",
                      _in.width, _in.height, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      AUDIOLIB_gainSmoothing_InitArgs kerInitArgs;
      kerInitArgs.funcStyle     = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate  = _config.samplingRate;
      kerInitArgs.isInterleaved = _in.isInterleave;
      // The kernel reads its ramp coefficients with vector loads: the handle must be 64-byte aligned
      _handle = handleAllocator().allocate64ByteAligned<char>(AUDIOLIB_gainSmoothing_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("GainSmoothing constructor", "failed to allocate gainSmoothing handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);
         AUDIOLIB_STATUS        status =
             AUDIOLIB_gainSmoothing_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_gainSmoothing_init_checkParams")) {
            status = AUDIOLIB_gainSmoothing_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         }
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_gainSmoothing_init")) {
            AUDIOLIB_gainSmoothing_SetArgs kerSetArgs;
            kerSetArgs.currentGain = _tunableParams.currentGain;
            kerSetArgs.targetGain  = _tunableParams.targetGain;
            kerSetArgs.fadeTime    = _tunableParams.fadeTime;
            kerSetArgs.fadeType    = static_cast<uint8_t>(_tunableParams.fadeType);
            status                 = AUDIOLIB_gainSmoothing_set(_handle.get(), &kerSetArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_gainSmoothing_set");
         }
      }
   }
}

template <typename T> void *GainSmoothing<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t GainSmoothing<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *GainSmoothing<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t GainSmoothing<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void GainSmoothing<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void GainSmoothing<T>::bypassCopy() noexcept
{
   const size_t rowBytes = _in.width * sizeof(T);

   if (_in.pBuf != _out.pBuf) {
      if ((_in.pitchBytes == rowBytes) && (_out.pitchBytes == rowBytes)) {
         (void) memcpy(_out.pBuf, _in.pBuf, rowBytes * _in.height);
      }
      else {
         uint8_t       *pDst = static_cast<uint8_t *>(_out.pBuf);
         const uint8_t *pSrc = static_cast<const uint8_t *>(_in.pBuf);

         for (size_t i = 0; i < _in.height; i++) {
            (void) memcpy(&pDst[i * _out.pitchBytes], &pSrc[i * _in.pitchBytes], rowBytes); // NOLINT(MISRA.PTR.ARITH)
         }
      }
   }
}

template <typename T> void GainSmoothing<T>::exec() noexcept
{
   if (_tunableParams.bypassMode != 0U) {
      bypassCopy();
   }
   else {
      AUDIOLIB_gainSmoothing_exec(_handle.get(), _in.pBuf, _out.pBuf);
   }
}

template <typename T>
bool GainSmoothing<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   GainSmoothingTunableParams<T> params = _tunableParams;

   if (value == nullptr) {
      errorCtx.setNullPointerError("GainSmoothing::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<GainSmoothingParamId>(paramId)) {
      case GainSmoothingParamId::kCurrentGain: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams",
                                              "Value size smaller than expected for kCurrentGain");
         }
         else {
            float currentGain;
            memcpy(&currentGain, value, sizeof(float));
            if ((currentGain < 0.0f) || (currentGain > 1000.0f)) {
               errorCtx.setInvalidParameterError("GainSmoothing::setParams", "currentGain is out of range");
            }
            else {
               params.currentGain = currentGain;
            }
         }
         break;
      }
      case GainSmoothingParamId::kTargetGain: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams",
                                              "Value size smaller than expected for kTargetGain");
         }
         else {
            float targetGain;
            memcpy(&targetGain, value, sizeof(float));
            if ((targetGain < 0.0f) || (targetGain > 1000.0f)) {
               errorCtx.setInvalidParameterError("GainSmoothing::setParams", "targetGain is out of range");
            }
            else {
               params.targetGain = targetGain;
            }
         }
         break;
      }
      case GainSmoothingParamId::kFadeTime: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams",
                                              "Value size smaller than expected for kFadeTime");
         }
         else {
            float fadeTime;
            memcpy(&fadeTime, value, sizeof(float));
            if ((fadeTime < 0.0f) || (fadeTime > 10000.0f)) {
               errorCtx.setInvalidParameterError("GainSmoothing::setParams", "fadeTime is out of range");
            }
            else {
               params.fadeTime = fadeTime;
            }
         }
         break;
      }
      case GainSmoothingParamId::kFadeType: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams",
                                              "Value size smaller than expected for kFadeType");
         }
         else {
            uint32_t fadeType;
            memcpy(&fadeType, value, sizeof(uint32_t));
            if (fadeType > 1U) {
               errorCtx.setInvalidParameterError("GainSmoothing::setParams", "fadeType is out of range");
            }
            else {
               params.fadeType = fadeType;
            }
         }
         break;
      }
      case GainSmoothingParamId::kBypass: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams",
                                              "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode;
            memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("GainSmoothing::setParams", "bypassMode is out of range");
            }
            else {
               params.bypassMode = bypassMode;
            }
         }
         break;
      }
      case GainSmoothingParamId::kAll: {
         const GainSmoothingTunableParams<T> *pParams = static_cast<const GainSmoothingTunableParams<T> *>(value);
         if (size < sizeof(GainSmoothingTunableParams<T>)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams", "bypassMode is out of range");
         }
         else if ((pParams->currentGain < 0.0f) || (pParams->currentGain > 1000.0f)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams", "currentGain is out of range");
         }
         else if ((pParams->targetGain < 0.0f) || (pParams->targetGain > 1000.0f)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams", "targetGain is out of range");
         }
         else if ((pParams->fadeTime < 0.0f) || (pParams->fadeTime > 10000.0f)) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams", "fadeTime is out of range");
         }
         else if (pParams->fadeType > 1U) {
            errorCtx.setInvalidParameterError("GainSmoothing::setParams", "fadeType is out of range");
         }
         else {
            params.bypassMode  = pParams->bypassMode;
            params.currentGain = pParams->currentGain;
            params.targetGain  = pParams->targetGain;
            params.fadeTime    = pParams->fadeTime;
            params.fadeType    = pParams->fadeType;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("GainSmoothing::setParams", "Invalid paramId");
         break;
      }
   }

   // The kernel holds the ramp: hand it the new values, so the next exec() starts a ramp from currentGain to targetGain
   if (errorCtx.isSuccess()) {
      if (paramId != static_cast<uint16_t>(GainSmoothingParamId::kBypass)) {
         AUDIOLIB_gainSmoothing_SetArgs kerSetArgs;
         kerSetArgs.currentGain = params.currentGain;
         kerSetArgs.targetGain  = params.targetGain;
         kerSetArgs.fadeTime    = params.fadeTime;
         kerSetArgs.fadeType    = static_cast<uint8_t>(params.fadeType);
         AUDIOLIB_STATUS status = AUDIOLIB_gainSmoothing_set(_handle.get(), &kerSetArgs);
         errorCtx.remapXLIBError(status, "AUDIOLIB_gainSmoothing_set");
      }
      if (errorCtx.isSuccess()) {
         _tunableParams = params;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool GainSmoothing<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("GainSmoothing::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("GainSmoothing::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<GainSmoothingParamId>(paramId)) {
      case GainSmoothingParamId::kCurrentGain: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.currentGain, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case GainSmoothingParamId::kTargetGain: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.targetGain, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case GainSmoothingParamId::kFadeTime: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.fadeTime, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case GainSmoothingParamId::kFadeType: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.fadeType, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case GainSmoothingParamId::kBypass: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case GainSmoothingParamId::kAll: {
         GainSmoothingTunableParams<T> *pParams = static_cast<GainSmoothingTunableParams<T> *>(value);
         if (*size < sizeof(GainSmoothingTunableParams<T>)) {
            errorCtx.setInvalidParameterError("GainSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            pParams->bypassMode  = _tunableParams.bypassMode;
            pParams->currentGain = _tunableParams.currentGain;
            pParams->targetGain  = _tunableParams.targetGain;
            pParams->fadeTime    = _tunableParams.fadeTime;
            pParams->fadeType    = _tunableParams.fadeType;
         }
         *size = sizeof(GainSmoothingTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("GainSmoothing::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class GainSmoothing<float>;

} // namespace AUDIOLIB
} // namespace TISP
