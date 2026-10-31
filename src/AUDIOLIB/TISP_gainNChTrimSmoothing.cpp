// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_gainNChTrimSmoothing.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {
template <typename T>
GainNChTrimSmoothing<T>::GainNChTrimSmoothing(const TISP_BufDescList                     *pInputs,
                                              const TISP_BufDescList                     *pOutputs,
                                              const GainNChTrimSmoothingConfig           *pConfig,
                                              const GainNChTrimSmoothingTunableParams<T> *pDefaultTunableParams,
                                              const char                                 *nodeName,
                                              uint32_t                                    nodeId,
                                              ErrorCtxt                                  &errorCtx)
    : _in{}, _out{}, _config{}, _tunableParams{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("GainNChTrimSmoothing constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("GainNChTrimSmoothing constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pTrimGains == nullptr) {
      errorCtx.setNullPointerError("GainNChTrimSmoothing constructor",
                                   "pointer pDefaultTunableParams->pTrimGains is null");
   }
   else {
      _config        = *pConfig;
      _tunableParams = *pDefaultTunableParams;
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "GainNChTrimSmoothing constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "GainNChTrimSmoothing constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_out.isInterleave != _in.isInterleave) {
         errorCtx.setInvalidParameterError("GainNChTrimSmoothing constructor", "output 0 layout must match input 0");
      }
      else if ((_config.samplingRate < 1.0f) || (_config.samplingRate > 192000.0f)) {
         errorCtx.setInvalidParameterError("GainNChTrimSmoothing constructor", "samplingRate is out of range");
      }
      else if (_tunableParams.bypassMode > 1U) {
         errorCtx.setInvalidParameterError("GainNChTrimSmoothing constructor", "bypassMode is out of range");
      }
      else if ((_tunableParams.masterGain < 0.0f) || (_tunableParams.masterGain > 1000.0f)) {
         errorCtx.setInvalidParameterError("GainNChTrimSmoothing constructor", "masterGain is out of range");
      }
      else if ((_tunableParams.fadeTime < 0.0f) || (_tunableParams.fadeTime > 10000.0f)) {
         errorCtx.setInvalidParameterError("GainNChTrimSmoothing constructor", "fadeTime is out of range");
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

      const size_t                           numChannels = (_in.isInterleave == 1U) ? _in.width : _in.height;
      AUDIOLIB_gainNChTrimSmoothing_InitArgs kerInitArgs;
      kerInitArgs.funcStyle     = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate  = _config.samplingRate;
      kerInitArgs.isInterleaved = _in.isInterleave;
      // The kernel reads its per-channel state with vector loads: the handle must be 64-byte aligned
      _handle =
          handleAllocator().allocate64ByteAligned<char>(AUDIOLIB_gainNChTrimSmoothing_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("GainNChTrimSmoothing constructor", "failed to allocate gainNChTrimSmoothing handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);
         AUDIOLIB_STATUS        status =
             AUDIOLIB_gainNChTrimSmoothing_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_gainNChTrimSmoothing_init_checkParams")) {
            status = AUDIOLIB_gainNChTrimSmoothing_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         }
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_gainNChTrimSmoothing_init")) {
            // Every channel starts at rest on its target gain, masterGain * trimGains[ch]
            float initialGains[AUDIOLIB_GAINNCHTRIMSMOOTHING_MAX_NUM_CHANNELS];
            for (size_t ch = 0U; ch < numChannels; ch++) {
               initialGains[ch] = _tunableParams.masterGain * _tunableParams.pTrimGains[ch];
            }
            AUDIOLIB_gainNChTrimSmoothing_SetArgs kerSetArgs;
            kerSetArgs.initialGain = initialGains;
            kerSetArgs.trimGain    = _tunableParams.pTrimGains;
            kerSetArgs.masterGain  = _tunableParams.masterGain;
            kerSetArgs.fadeTime    = _tunableParams.fadeTime;
            kerSetArgs.fadeType    = static_cast<uint8_t>(_tunableParams.fadeType);
            status                 = AUDIOLIB_gainNChTrimSmoothing_set(_handle.get(), &kerSetArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_gainNChTrimSmoothing_set");
         }
      }
   }
}

template <typename T> void *GainNChTrimSmoothing<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t GainNChTrimSmoothing<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *GainNChTrimSmoothing<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t GainNChTrimSmoothing<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void GainNChTrimSmoothing<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void GainNChTrimSmoothing<T>::bypassCopy() noexcept
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

template <typename T> void GainNChTrimSmoothing<T>::exec() noexcept
{
   if (_tunableParams.bypassMode != 0U) {
      bypassCopy();
   }
   else {
      AUDIOLIB_gainNChTrimSmoothing_exec(_handle.get(), _in.pBuf, _out.pBuf);
   }
}

template <typename T>
bool GainNChTrimSmoothing<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("GainNChTrimSmoothing::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<GainNChTrimSmoothingParamId>(paramId)) {
      case GainNChTrimSmoothingParamId::kBypass: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams",
                                              "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode;
            memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams", "bypassMode is out of range");
            }
            else {
               _tunableParams.bypassMode = bypassMode;
            }
         }
         break;
      }
      case GainNChTrimSmoothingParamId::kTrimGains: {
         size_t expectedSize =
             (static_cast<size_t>(((_in.isInterleave == 1U) ? _in.width : _in.height))) * sizeof(float);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams",
                                              "Value size smaller than expected for kTrimGains");
         }
         else {
            memcpy(_tunableParams.pTrimGains, value, expectedSize);
         }
         break;
      }
      case GainNChTrimSmoothingParamId::kMasterGain: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams",
                                              "Value size smaller than expected for kMasterGain");
         }
         else {
            float masterGain;
            memcpy(&masterGain, value, sizeof(float));
            if ((masterGain < 0.0f) || (masterGain > 1000.0f)) {
               errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams", "masterGain is out of range");
            }
            else {
               _tunableParams.masterGain = masterGain;
            }
         }
         break;
      }
      case GainNChTrimSmoothingParamId::kFadeTime: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams",
                                              "Value size smaller than expected for kFadeTime");
         }
         else {
            float fadeTime;
            memcpy(&fadeTime, value, sizeof(float));
            if ((fadeTime < 0.0f) || (fadeTime > 10000.0f)) {
               errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams", "fadeTime is out of range");
            }
            else {
               _tunableParams.fadeTime = fadeTime;
            }
         }
         break;
      }
      case GainNChTrimSmoothingParamId::kFadeType: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams",
                                              "Value size smaller than expected for kFadeType");
         }
         else {
            memcpy(&_tunableParams.fadeType, value, sizeof(uint32_t));
         }
         break;
      }
      case GainNChTrimSmoothingParamId::kAll: {
         const GainNChTrimSmoothingTunableParams<T> *pParams =
             static_cast<const GainNChTrimSmoothingTunableParams<T> *>(value);
         if (size < sizeof(GainNChTrimSmoothingTunableParams<T>)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams",
                                              "Value size smaller than expected for kAll");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams", "bypassMode is out of range");
         }
         else if (pParams->pTrimGains == nullptr) {
            errorCtx.setNullPointerError("GainNChTrimSmoothing::setParams", "pointer pTrimGains is null");
         }
         else if ((pParams->masterGain < 0.0f) || (pParams->masterGain > 1000.0f)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams", "masterGain is out of range");
         }
         else if ((pParams->fadeTime < 0.0f) || (pParams->fadeTime > 10000.0f)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams", "fadeTime is out of range");
         }
         else {
            _tunableParams.bypassMode = pParams->bypassMode;
            if (pParams->pTrimGains != _tunableParams.pTrimGains) {
               memcpy(_tunableParams.pTrimGains, pParams->pTrimGains,
                      (static_cast<size_t>(((_in.isInterleave == 1U) ? _in.width : _in.height))) * sizeof(float));
            }
            _tunableParams.masterGain = pParams->masterGain;
            _tunableParams.fadeTime   = pParams->fadeTime;
            _tunableParams.fadeType   = pParams->fadeType;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("GainNChTrimSmoothing::setParams", "Invalid paramId");
         break;
      }
   }

   // The kernel holds the gains: hand it the new values, so it starts a fade for every changed target
   if (errorCtx.isSuccess() && (paramId != static_cast<uint16_t>(GainNChTrimSmoothingParamId::kBypass))) {
      AUDIOLIB_gainNChTrimSmoothing_SetArgs kerSetArgs;
      kerSetArgs.initialGain = nullptr;
      kerSetArgs.trimGain    = _tunableParams.pTrimGains;
      kerSetArgs.masterGain  = _tunableParams.masterGain;
      kerSetArgs.fadeTime    = _tunableParams.fadeTime;
      kerSetArgs.fadeType    = static_cast<uint8_t>(_tunableParams.fadeType);
      AUDIOLIB_STATUS status = AUDIOLIB_gainNChTrimSmoothing_set(_handle.get(), &kerSetArgs);
      errorCtx.remapXLIBError(status, "AUDIOLIB_gainNChTrimSmoothing_set");
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool GainNChTrimSmoothing<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("GainNChTrimSmoothing::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("GainNChTrimSmoothing::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<GainNChTrimSmoothingParamId>(paramId)) {
      case GainNChTrimSmoothingParamId::kBypass: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case GainNChTrimSmoothingParamId::kTrimGains: {
         size_t expectedSize =
             (static_cast<size_t>(((_in.isInterleave == 1U) ? _in.width : _in.height))) * sizeof(float);
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, _tunableParams.pTrimGains, expectedSize);
         }
         *size = expectedSize;
         break;
      }
      case GainNChTrimSmoothingParamId::kMasterGain: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.masterGain, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case GainNChTrimSmoothingParamId::kFadeTime: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.fadeTime, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case GainNChTrimSmoothingParamId::kFadeType: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.fadeType, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case GainNChTrimSmoothingParamId::kAll: {
         GainNChTrimSmoothingTunableParams<T> *pParams = static_cast<GainNChTrimSmoothingTunableParams<T> *>(value);
         if (*size < sizeof(GainNChTrimSmoothingTunableParams<T>)) {
            errorCtx.setInvalidParameterError("GainNChTrimSmoothing::getParams", "Value size smaller than expected");
         }
         else if (pParams->pTrimGains == nullptr) {
            errorCtx.setNullPointerError("GainNChTrimSmoothing::getParams", "pointer pTrimGains is null");
         }
         else {
            pParams->bypassMode = _tunableParams.bypassMode;
            if (pParams->pTrimGains != _tunableParams.pTrimGains) {
               memcpy(pParams->pTrimGains, _tunableParams.pTrimGains,
                      (static_cast<size_t>(((_in.isInterleave == 1U) ? _in.width : _in.height))) * sizeof(float));
            }
            pParams->masterGain = _tunableParams.masterGain;
            pParams->fadeTime   = _tunableParams.fadeTime;
            pParams->fadeType   = _tunableParams.fadeType;
         }
         *size = sizeof(GainNChTrimSmoothingTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("GainNChTrimSmoothing::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class GainNChTrimSmoothing<float>;

} // namespace AUDIOLIB
} // namespace TISP
