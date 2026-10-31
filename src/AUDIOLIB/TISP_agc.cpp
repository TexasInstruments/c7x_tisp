// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_agc.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
Agc<T>::Agc(const TISP_BufDescList    *pInputs,
            const TISP_BufDescList    *pOutputs,
            const AgcConfig           *pConfig,
            const AgcTunableParams<T> *pDefaultTunableParams,
            const char                *nodeName,
            uint32_t                   nodeId,
            ErrorCtxt                 &errorCtx)
    : _in{}, _out{}, _config{}, _tunableParams{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("Agc constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("Agc constructor", "default params pointer pDefaultTunableParams is null");
   }
   else {
      _config        = *pConfig;
      _tunableParams = *pDefaultTunableParams;
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Agc constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Agc constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("Agc constructor", "input 0 must be planar");
      }
      else if (_out.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("Agc constructor", "output 0 must be planar");
      }
      else if ((_config.samplingRate < 1) || (_config.samplingRate > 192000)) {
         errorCtx.setInvalidParameterError("Agc constructor", "samplingRate is out of range");
      }
      else if ((_config.currentGain < 0.0f) || (_config.currentGain > 1000.0f)) {
         errorCtx.setInvalidParameterError("Agc constructor", "currentGain is out of range");
      }
      else if ((_tunableParams.targetLevel < -50.0f) || (_tunableParams.targetLevel > 50.0f)) {
         errorCtx.setInvalidParameterError("Agc constructor", "targetLevel is out of range");
      }
      else if ((_tunableParams.maxAttenuation < 0.0f) || (_tunableParams.maxAttenuation > 100.0f)) {
         errorCtx.setInvalidParameterError("Agc constructor", "maxAttenuation is out of range");
      }
      else if ((_tunableParams.maxGain < 0.0f) || (_tunableParams.maxGain > 50.0f)) {
         errorCtx.setInvalidParameterError("Agc constructor", "maxGain is out of range");
      }
      else if ((_tunableParams.ratio < 1.0f) || (_tunableParams.ratio > 100.0f)) {
         errorCtx.setInvalidParameterError("Agc constructor", "ratio is out of range");
      }
      else if ((_tunableParams.activationThreshold < -100.0f) || (_tunableParams.activationThreshold > 0.0f)) {
         errorCtx.setInvalidParameterError("Agc constructor", "activationThreshold is out of range");
      }
      else if ((_tunableParams.smoothingTime < 1.0f) || (_tunableParams.smoothingTime > 5000.0f)) {
         errorCtx.setInvalidParameterError("Agc constructor", "smoothingTime is out of range");
      }
      else if (_tunableParams.enableRecovery > 1U) {
         errorCtx.setInvalidParameterError("Agc constructor", "enableRecovery is out of range");
      }
      else if ((_tunableParams.recoveryRate < 0.0f) || (_tunableParams.recoveryRate > 100.0f)) {
         errorCtx.setInvalidParameterError("Agc constructor", "recoveryRate is out of range");
      }
      else if (_tunableParams.bypassMode > 1U) {
         errorCtx.setInvalidParameterError("Agc constructor", "bypassMode is out of range");
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

      AUDIOLIB_agc_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate = _config.samplingRate;
      kerInitArgs.currentGain  = _config.currentGain;

      _handle = make_unique_handle<char>(AUDIOLIB_agc_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Agc constructor", "failed to allocate agc handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

         AUDIOLIB_STATUS status =
             AUDIOLIB_agc_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_agc_init_checkParams")) {
            status = AUDIOLIB_agc_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         }
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_agc_init")) {
            AUDIOLIB_agc_SetArgs kerSetArgs;
            kerSetArgs.targetLevel         = _tunableParams.targetLevel;
            kerSetArgs.maxAttenuation      = _tunableParams.maxAttenuation;
            kerSetArgs.maxGain             = _tunableParams.maxGain;
            kerSetArgs.ratio               = _tunableParams.ratio;
            kerSetArgs.activationThreshold = _tunableParams.activationThreshold;
            kerSetArgs.smoothingTime       = _tunableParams.smoothingTime;
            kerSetArgs.enableRecovery      = static_cast<uint8_t>(_tunableParams.enableRecovery);
            kerSetArgs.recoveryRate        = _tunableParams.recoveryRate;
            status                         = AUDIOLIB_agc_set(_handle.get(), &kerSetArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_agc_set");
         }
      }
   }
}

template <typename T> void *Agc<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t Agc<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *Agc<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Agc<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Agc<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void Agc<T>::bypassCopy() noexcept
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

template <typename T> void Agc<T>::exec() noexcept
{
   if (_tunableParams.bypassMode != 0U) {
      bypassCopy();
   }
   else {
      AUDIOLIB_agc_exec(_handle.get(), _in.pBuf, _out.pBuf);
   }
}

template <typename T> bool Agc<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Agc::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<AgcParamId>(paramId)) {
      case AgcParamId::kTargetLevel: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kTargetLevel");
         }
         else {
            float targetLevel;
            memcpy(&targetLevel, value, sizeof(float));
            if ((targetLevel < -50.0f) || (targetLevel > 50.0f)) {
               errorCtx.setInvalidParameterError("Agc::setParams", "targetLevel is out of range");
            }
            else {
               _tunableParams.targetLevel = targetLevel;
            }
         }
         break;
      }
      case AgcParamId::kMaxAttenuation: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kMaxAttenuation");
         }
         else {
            float maxAttenuation;
            memcpy(&maxAttenuation, value, sizeof(float));
            if ((maxAttenuation < 0.0f) || (maxAttenuation > 100.0f)) {
               errorCtx.setInvalidParameterError("Agc::setParams", "maxAttenuation is out of range");
            }
            else {
               _tunableParams.maxAttenuation = maxAttenuation;
            }
         }
         break;
      }
      case AgcParamId::kMaxGain: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kMaxGain");
         }
         else {
            float maxGain;
            memcpy(&maxGain, value, sizeof(float));
            if ((maxGain < 0.0f) || (maxGain > 50.0f)) {
               errorCtx.setInvalidParameterError("Agc::setParams", "maxGain is out of range");
            }
            else {
               _tunableParams.maxGain = maxGain;
            }
         }
         break;
      }
      case AgcParamId::kRatio: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kRatio");
         }
         else {
            float ratio;
            memcpy(&ratio, value, sizeof(float));
            if ((ratio < 1.0f) || (ratio > 100.0f)) {
               errorCtx.setInvalidParameterError("Agc::setParams", "ratio is out of range");
            }
            else {
               _tunableParams.ratio = ratio;
            }
         }
         break;
      }
      case AgcParamId::kActivationThreshold: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::setParams",
                                              "Value size smaller than expected for kActivationThreshold");
         }
         else {
            float activationThreshold;
            memcpy(&activationThreshold, value, sizeof(float));
            if ((activationThreshold < -100.0f) || (activationThreshold > 0.0f)) {
               errorCtx.setInvalidParameterError("Agc::setParams", "activationThreshold is out of range");
            }
            else {
               _tunableParams.activationThreshold = activationThreshold;
            }
         }
         break;
      }
      case AgcParamId::kSmoothingTime: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kSmoothingTime");
         }
         else {
            float smoothingTime;
            memcpy(&smoothingTime, value, sizeof(float));
            if ((smoothingTime < 1.0f) || (smoothingTime > 5000.0f)) {
               errorCtx.setInvalidParameterError("Agc::setParams", "smoothingTime is out of range");
            }
            else {
               _tunableParams.smoothingTime = smoothingTime;
            }
         }
         break;
      }
      case AgcParamId::kEnableRecovery: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kEnableRecovery");
         }
         else {
            uint32_t enableRecovery;
            memcpy(&enableRecovery, value, sizeof(uint32_t));
            if (enableRecovery > 1U) {
               errorCtx.setInvalidParameterError("Agc::setParams", "enableRecovery is out of range");
            }
            else {
               _tunableParams.enableRecovery = enableRecovery;
            }
         }
         break;
      }
      case AgcParamId::kRecoveryRate: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kRecoveryRate");
         }
         else {
            float recoveryRate;
            memcpy(&recoveryRate, value, sizeof(float));
            if ((recoveryRate < 0.0f) || (recoveryRate > 100.0f)) {
               errorCtx.setInvalidParameterError("Agc::setParams", "recoveryRate is out of range");
            }
            else {
               _tunableParams.recoveryRate = recoveryRate;
            }
         }
         break;
      }
      case AgcParamId::kBypass: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode;
            memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("Agc::setParams", "bypassMode is out of range");
            }
            else {
               _tunableParams.bypassMode = bypassMode;
            }
         }
         break;
      }
      case AgcParamId::kAll: {
         const AgcTunableParams<T> *pParams = static_cast<const AgcTunableParams<T> *>(value);
         if (size < sizeof(AgcTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "Value size smaller than expected for kAll");
         }
         else if ((pParams->targetLevel < -50.0f) || (pParams->targetLevel > 50.0f)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "targetLevel is out of range");
         }
         else if ((pParams->maxAttenuation < 0.0f) || (pParams->maxAttenuation > 100.0f)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "maxAttenuation is out of range");
         }
         else if ((pParams->maxGain < 0.0f) || (pParams->maxGain > 50.0f)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "maxGain is out of range");
         }
         else if ((pParams->ratio < 1.0f) || (pParams->ratio > 100.0f)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "ratio is out of range");
         }
         else if ((pParams->activationThreshold < -100.0f) || (pParams->activationThreshold > 0.0f)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "activationThreshold is out of range");
         }
         else if ((pParams->smoothingTime < 1.0f) || (pParams->smoothingTime > 5000.0f)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "smoothingTime is out of range");
         }
         else if (pParams->enableRecovery > 1U) {
            errorCtx.setInvalidParameterError("Agc::setParams", "enableRecovery is out of range");
         }
         else if ((pParams->recoveryRate < 0.0f) || (pParams->recoveryRate > 100.0f)) {
            errorCtx.setInvalidParameterError("Agc::setParams", "recoveryRate is out of range");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("Agc::setParams", "bypassMode is out of range");
         }
         else {
            _tunableParams.targetLevel         = pParams->targetLevel;
            _tunableParams.maxAttenuation      = pParams->maxAttenuation;
            _tunableParams.maxGain             = pParams->maxGain;
            _tunableParams.ratio               = pParams->ratio;
            _tunableParams.activationThreshold = pParams->activationThreshold;
            _tunableParams.smoothingTime       = pParams->smoothingTime;
            _tunableParams.enableRecovery      = pParams->enableRecovery;
            _tunableParams.recoveryRate        = pParams->recoveryRate;
            _tunableParams.bypassMode          = pParams->bypassMode;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Agc::setParams", "Invalid paramId");
         break;
      }
   }

   // The kernel holds the parameters: hand it the new values; its state (gain, envelope) is kept
   if (errorCtx.isSuccess() && (paramId != static_cast<uint16_t>(AgcParamId::kBypass))) {
      AUDIOLIB_agc_SetArgs kerSetArgs;
      kerSetArgs.targetLevel         = _tunableParams.targetLevel;
      kerSetArgs.maxAttenuation      = _tunableParams.maxAttenuation;
      kerSetArgs.maxGain             = _tunableParams.maxGain;
      kerSetArgs.ratio               = _tunableParams.ratio;
      kerSetArgs.activationThreshold = _tunableParams.activationThreshold;
      kerSetArgs.smoothingTime       = _tunableParams.smoothingTime;
      kerSetArgs.enableRecovery      = static_cast<uint8_t>(_tunableParams.enableRecovery);
      kerSetArgs.recoveryRate        = _tunableParams.recoveryRate;
      AUDIOLIB_STATUS status         = AUDIOLIB_agc_set(_handle.get(), &kerSetArgs);
      errorCtx.remapXLIBError(status, "AUDIOLIB_agc_set");
   }

   return errorCtx.isSuccess();
}

template <typename T> bool Agc<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Agc::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("Agc::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<AgcParamId>(paramId)) {
      case AgcParamId::kTargetLevel: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.targetLevel, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case AgcParamId::kMaxAttenuation: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.maxAttenuation, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case AgcParamId::kMaxGain: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.maxGain, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case AgcParamId::kRatio: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.ratio, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case AgcParamId::kActivationThreshold: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.activationThreshold, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case AgcParamId::kSmoothingTime: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.smoothingTime, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case AgcParamId::kEnableRecovery: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.enableRecovery, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case AgcParamId::kRecoveryRate: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.recoveryRate, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case AgcParamId::kBypass: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case AgcParamId::kAll: {
         AgcTunableParams<T> *pParams = static_cast<AgcTunableParams<T> *>(value);
         if (*size < sizeof(AgcTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Agc::getParams", "Value size smaller than expected");
         }
         else {
            pParams->targetLevel         = _tunableParams.targetLevel;
            pParams->maxAttenuation      = _tunableParams.maxAttenuation;
            pParams->maxGain             = _tunableParams.maxGain;
            pParams->ratio               = _tunableParams.ratio;
            pParams->activationThreshold = _tunableParams.activationThreshold;
            pParams->smoothingTime       = _tunableParams.smoothingTime;
            pParams->enableRecovery      = _tunableParams.enableRecovery;
            pParams->recoveryRate        = _tunableParams.recoveryRate;
            pParams->bypassMode          = _tunableParams.bypassMode;
         }
         *size = sizeof(AgcTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Agc::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class Agc<float>;
template class Agc<double>;

} // namespace AUDIOLIB
} // namespace TISP
