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

template <typename T>
Mute<T>::Mute(const TISP_BufDescList     *pInputs,
              const TISP_BufDescList     *pOutputs,
              const MuteConfig           *pConfig,
              const MuteTunableParams<T> *pDefaultTunableParams,
              const char                 *nodeName,
              uint32_t                    nodeId,
              ErrorCtxt                  &errorCtx)
    : _in{}, _out{}, _samplingRate(0), _isMute(0U), _fadeTime(0.0F), _fadeType(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("Mute constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("Mute constructor", "default params pointer pDefaultTunableParams is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Mute constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Mute constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("Mute constructor", "input and output interleave must match");
      }
      else if (pConfig->samplingRate < 1) {
         errorCtx.setInvalidParameterError("Mute constructor", "samplingRate is out of range");
      }
      else if (pDefaultTunableParams->isMute > 1U) {
         errorCtx.setInvalidParameterError("Mute constructor", "isMute is out of range");
      }
      else if (pDefaultTunableParams->fadeTime < 0.0F) {
         errorCtx.setInvalidParameterError("Mute constructor", "fadeTime is out of range");
      }
      else if (pDefaultTunableParams->fadeType > 3U) {
         errorCtx.setInvalidParameterError("Mute constructor", "fadeType is out of range");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _samplingRate = pConfig->samplingRate;
      _isMute       = pDefaultTunableParams->isMute;
      _fadeTime     = pDefaultTunableParams->fadeTime;
      _fadeType     = pDefaultTunableParams->fadeType;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "width=%zu,height=%zu,isInterleave=%u,samplingRate=%d", _in.width, _in.height,
                      static_cast<uint32_t>(_in.isInterleave), _samplingRate);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      AUDIOLIB_mute_InitArgs kerInitArgs;
      kerInitArgs.funcStyle     = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate  = _samplingRate;
      kerInitArgs.isInterleaved = _in.isInterleave;

      _handle = make_unique_handle<char>(AUDIOLIB_mute_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("Mute constructor", "failed to allocate mute handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

         AUDIOLIB_STATUS status =
             AUDIOLIB_mute_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_mute_init_checkParams")) {
            status = AUDIOLIB_mute_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "AUDIOLIB_mute_init")) {
               AUDIOLIB_mute_SetArgs kerSetArgs;
               kerSetArgs.isMute   = _isMute;
               kerSetArgs.fadeTime = _fadeTime;
               kerSetArgs.fadeType = _fadeType;
               status              = AUDIOLIB_mute_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(status, "AUDIOLIB_mute_set");
            }
         }
      }
   }
}

template <typename T> void *Mute<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t Mute<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *Mute<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Mute<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Mute<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void Mute<T>::exec() noexcept { AUDIOLIB_mute_exec(_handle.get(), _in.pBuf, _out.pBuf); }

template <typename T> bool Mute<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Mute::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<MuteParamId>(paramId)) {
      case MuteParamId::kIsMute: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Mute::setParams", "Value size smaller than expected for kIsMute");
         }
         else {
            uint32_t isMute;
            memcpy(&isMute, value, sizeof(uint32_t));
            if (isMute > 1U) {
               errorCtx.setInvalidParameterError("Mute::setParams", "isMute is out of range");
            }
            else {
               _isMute = isMute;
            }
         }
         break;
      }
      case MuteParamId::kFadeTime: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Mute::setParams", "Value size smaller than expected for kFadeTime");
         }
         else {
            float fadeTime;
            memcpy(&fadeTime, value, sizeof(float));
            if (fadeTime < 0.0F) {
               errorCtx.setInvalidParameterError("Mute::setParams", "fadeTime is out of range");
            }
            else {
               _fadeTime = fadeTime;
            }
         }
         break;
      }
      case MuteParamId::kFadeType: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Mute::setParams", "Value size smaller than expected for kFadeType");
         }
         else {
            uint32_t fadeType;
            memcpy(&fadeType, value, sizeof(uint32_t));
            if (fadeType > 3U) {
               errorCtx.setInvalidParameterError("Mute::setParams", "fadeType is out of range");
            }
            else {
               _fadeType = fadeType;
            }
         }
         break;
      }
      case MuteParamId::kAll: {
         const MuteTunableParams<T> *pParams = static_cast<const MuteTunableParams<T> *>(value);
         if (size < sizeof(MuteTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Mute::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->isMute > 1U) {
            errorCtx.setInvalidParameterError("Mute::setParams", "isMute is out of range");
         }
         else if (pParams->fadeTime < 0.0F) {
            errorCtx.setInvalidParameterError("Mute::setParams", "fadeTime is out of range");
         }
         else if (pParams->fadeType > 3U) {
            errorCtx.setInvalidParameterError("Mute::setParams", "fadeType is out of range");
         }
         else {
            _isMute   = pParams->isMute;
            _fadeTime = pParams->fadeTime;
            _fadeType = pParams->fadeType;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Mute::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_mute_SetArgs kerSetArgs;
         kerSetArgs.isMute            = _isMute;
         kerSetArgs.fadeTime          = _fadeTime;
         kerSetArgs.fadeType          = _fadeType;
         const AUDIOLIB_STATUS status = AUDIOLIB_mute_set(_handle.get(), &kerSetArgs);
         errorCtx.remapXLIBError(status, "AUDIOLIB_mute_set");
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> bool Mute<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Mute::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("Mute::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<MuteParamId>(paramId)) {
      case MuteParamId::kIsMute: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Mute::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_isMute, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case MuteParamId::kFadeTime: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Mute::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_fadeTime, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case MuteParamId::kFadeType: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Mute::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_fadeType, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case MuteParamId::kAll: {
         MuteTunableParams<T> *pParams = static_cast<MuteTunableParams<T> *>(value);
         if (*size < sizeof(MuteTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Mute::getParams", "Value size smaller than expected");
         }
         else {
            pParams->isMute   = _isMute;
            pParams->fadeTime = _fadeTime;
            pParams->fadeType = _fadeType;
         }
         *size = sizeof(MuteTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Mute::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class Mute<float>;

} // namespace AUDIOLIB
} // namespace TISP
