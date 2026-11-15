// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_isolateNCh.hpp>
#include <cstring>
#include <string>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::IsolateNCh constructor]

template <typename T>
IsolateNCh<T>::IsolateNCh(const TISP_BufDescList           *pInputs,
                          const TISP_BufDescList           *pOutputs,
                          const IsolateNChConfig           *pConfig,
                          const IsolateNChTunableParams<T> *pDefaultTunableParams,
                          const char                       *nodeName,
                          uint32_t                          nodeId,
                          ErrorCtxt                        &errorCtx)
    : _in{}, _out{}, _inChannels(0U), _samplingRate(0), _pIsIsolated(nullptr), _bypassMode(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("IsolateNCh constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("IsolateNCh constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pIsIsolated == nullptr) {
      errorCtx.setNullPointerError("IsolateNCh constructor",
                                   "isolate pointer pDefaultTunableParams->pIsIsolated is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "IsolateNCh constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "IsolateNCh constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("IsolateNCh constructor", "input and output interleave must match");
      }
      else if (pConfig->samplingRate <= 0) {
         errorCtx.setInvalidParameterError("IsolateNCh constructor", "samplingRate must be greater than zero");
      }
      else if (pDefaultTunableParams->bypassMode > 1U) {
         errorCtx.setInvalidParameterError("IsolateNCh constructor", "bypass mode must be either zero or one");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // One isolate flag per channel: channels are columns when interleaved, rows otherwise
      _inChannels   = (_in.isInterleave == 1U) ? _in.width : _in.height;
      _samplingRate = pConfig->samplingRate;
      _pIsIsolated  = pDefaultTunableParams->pIsIsolated;
      _bypassMode   = pDefaultTunableParams->bypassMode;

      if (_inChannels == 0U) {
         errorCtx.setInvalidParameterError("IsolateNCh constructor", "number of channels must be greater than zero");
      }

      for (size_t ch = 0U; ch < _inChannels; ch++) {
         if (_pIsIsolated[ch] > 1U) {
            errorCtx.setInvalidParameterError("IsolateNCh constructor", "isolate flags must be either zero or one");
         }
      }
   }

   if (errorCtx.isSuccess()) {

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numChannels=%zu,samplingRate=%d", _inChannels,
                      _samplingRate);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_muteNCh from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_muteNCh_InitArgs kerInitArgs;
      kerInitArgs.funcStyle     = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate  = _samplingRate;
      kerInitArgs.isInterleaved = _in.isInterleave;
      kerInitArgs.numChannels   = static_cast<uint32_t>(_inChannels);

      _setArgsBuf   = make_unique_handle<char>(sizeof(AUDIOLIB_muteNCh_SetArgs));
      _muteFlagsBuf = make_unique_handle<char>(_inChannels * sizeof(uint32_t));

      if (!_setArgsBuf || !_muteFlagsBuf) {
         errorCtx.setMemoryError("IsolateNCh constructor", "failed to allocate _setArgsBuf or _muteFlagsBuf");
      }
      else {
         uint32_t *pMuteFlags = reinterpret_cast<uint32_t *>(_muteFlagsBuf.get());
         // While any channel is isolated, every channel that is not isolated is muted (mute = 1, target gain 0);
         // while none is isolated, no channel is muted (mute = 0, target gain 1)
         bool anyIsolated = false;
         for (size_t ch = 0U; ch < _inChannels; ch++) {
            anyIsolated = anyIsolated || (_pIsIsolated[ch] == 1U);
         }
         for (size_t ch = 0U; ch < _inChannels; ch++) {
            pMuteFlags[ch] = (anyIsolated && (_pIsIsolated[ch] == 0U)) ? 1U : 0U;
         }

         AUDIOLIB_muteNCh_SetArgs *pSetArgs = reinterpret_cast<AUDIOLIB_muteNCh_SetArgs *>(_setArgsBuf.get());
         pSetArgs->isMute                   = pMuteFlags;
         pSetArgs->fadeTime                 = pDefaultTunableParams->fadeTime;
         pSetArgs->fadeType                 = pDefaultTunableParams->fadeType;

         _handle = make_unique_handle<char>(AUDIOLIB_muteNCh_getHandleSize(&kerInitArgs));

         if (!_handle) {
            errorCtx.setMemoryError("IsolateNCh constructor", "failed to allocate muteNCh handle");
         }
         else {
            AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
            AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

            /******************************************************************************/
            /* Validate parameters before init                                            */
            /******************************************************************************/

            AUDIOLIB_STATUS status =
                AUDIOLIB_muteNCh_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);

            /******************************************************************************/
            /* Call init function from AUDIOLIB to for muteNCh to set SE/SA params             */
            /******************************************************************************/

            if (errorCtx.remapXLIBError(status, "AUDIOLIB_muteNCh_init_checkParams")) {
               status = AUDIOLIB_muteNCh_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
               if (errorCtx.remapXLIBError(status, "AUDIOLIB_muteNCh_init")) {
                  // The set with the fade type checks it; a hard set (fadeType 2) then puts every channel at rest
                  // on its gain. The next setParams hands the kernel the fade type again.
                  status = AUDIOLIB_muteNCh_set(_handle.get(), pSetArgs);
                  if (errorCtx.remapXLIBError(status, "AUDIOLIB_muteNCh_set")) {
                     AUDIOLIB_muteNCh_SetArgs hardSetArgs = *pSetArgs;
                     hardSetArgs.fadeType                 = 2U;
                     status                               = AUDIOLIB_muteNCh_set(_handle.get(), &hardSetArgs);
                     errorCtx.remapXLIBError(status, "AUDIOLIB_muteNCh_set");
                  }
               }
            }
         }
      }
   }
}

template <typename T> void *IsolateNCh<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *IsolateNCh<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t IsolateNCh<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t IsolateNCh<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::IsolateNCh setAddr]
template <typename T> void IsolateNCh<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void IsolateNCh<T>::bypassCopy() noexcept
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

template <typename T> void IsolateNCh<T>::exec() noexcept
{
   if (_bypassMode != 0U) {
      bypassCopy();
   }
   else {
      AUDIOLIB_muteNCh_exec(_handle.get(), _in.pBuf, _out.pBuf);
   }
}

template <typename T>
bool IsolateNCh<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("IsolateNCh::setParams", "value pointer is null");
   }
   else if (!_setArgsBuf) {
      errorCtx.setMemoryError("IsolateNCh::setParams", "_setArgsBuf is null");
   }
   else {
      auto                    *pSetArgs = reinterpret_cast<AUDIOLIB_muteNCh_SetArgs *>(_setArgsBuf.get());
      AUDIOLIB_muteNCh_SetArgs setArgs  = {0};
      memcpy(&setArgs, pSetArgs, sizeof(AUDIOLIB_muteNCh_SetArgs));

      const char *paramName = "Unknown";

      /* Bypass is node local state and is not forwarded to the kernel. */
      bool      applyToKernel     = true;
      uint32_t  pendingBypassMode = _bypassMode;
      uint32_t *pNewIsolated      = _pIsIsolated;

      switch (static_cast<IsolateNChParamId>(paramId)) {
      case IsolateNChParamId::kIsIsolated: {
         size_t expectedSize = (_inChannels * sizeof(uint32_t));
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("IsolateNCh::setParams",
                                              "Value size smaller than expected for kIsIsolated");
         }
         else {
            pNewIsolated = static_cast<uint32_t *>(value);
            paramName    = "kIsIsolated";
         }
         break;
      }
      case IsolateNChParamId::kFadeTime: {
         if (size < sizeof(setArgs.fadeTime)) {
            errorCtx.setInvalidParameterError("IsolateNCh::setParams",
                                              "Value size smaller than expected for kFadeTime");
         }
         else {
            memcpy(&setArgs.fadeTime, value, sizeof(setArgs.fadeTime));
            paramName = "kFadeTime";
         }
         break;
      }
      case IsolateNChParamId::kFadeType: {
         if (size < sizeof(setArgs.fadeType)) {
            errorCtx.setInvalidParameterError("IsolateNCh::setParams",
                                              "Value size smaller than expected for kFadeType");
         }
         else {
            memcpy(&setArgs.fadeType, value, sizeof(setArgs.fadeType));
            paramName = "kFadeType";
         }
         break;
      }
      case IsolateNChParamId::kBypass: {
         applyToKernel = false;
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("IsolateNCh::setParams", "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode = 0U;
            memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("IsolateNCh::setParams", "bypass mode must be either zero or one");
            }
            else {
               _bypassMode = bypassMode;
            }
         }
         break;
      }
      case IsolateNChParamId::kAll: {
         const IsolateNChTunableParams<T> *pParams = static_cast<const IsolateNChTunableParams<T> *>(value);
         if (size < sizeof(IsolateNChTunableParams<T>)) {
            errorCtx.setInvalidParameterError("IsolateNCh::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pIsIsolated == nullptr) {
            errorCtx.setNullPointerError("IsolateNCh::setParams", "isolate pointer pIsIsolated is null");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("IsolateNCh::setParams", "bypass mode must be either zero or one");
         }
         else {
            pNewIsolated      = pParams->pIsIsolated;
            setArgs.fadeTime  = pParams->fadeTime;
            setArgs.fadeType  = pParams->fadeType;
            pendingBypassMode = pParams->bypassMode;
            paramName         = "kAll";
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("IsolateNCh::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess() && applyToKernel) {
         // Check every flag before any is used, so a rejected update leaves the caller's array unchanged
         bool anyIsolated = false;
         for (size_t ch = 0U; ch < _inChannels; ch++) {
            if (pNewIsolated[ch] > 1U) {
               errorCtx.setInvalidParameterError("IsolateNCh::setParams", "isolate flags must be either zero or one");
            }
            anyIsolated = anyIsolated || (pNewIsolated[ch] == 1U);
         }
         if (errorCtx.isSuccess()) {
            // While any channel is isolated, every channel that is not isolated is muted (mute = 1, target gain 0)
            for (size_t ch = 0U; ch < _inChannels; ch++) {
               setArgs.isMute[ch] = (anyIsolated && (pNewIsolated[ch] == 0U)) ? 1U : 0U;
            }
            AUDIOLIB_STATUS status = AUDIOLIB_muteNCh_set(_handle.get(), &setArgs);
            std::string     msg    = std::string("AUDIOLIB_muteNCh_set with ") + paramName;
            (void) errorCtx.remapXLIBError(status, msg.c_str());
            if (!errorCtx.isSuccess()) {
               // setArgs.isMute aliases the live mute buffer: put back the flags of the last accepted state
               bool anyOld = false;
               for (size_t ch = 0U; ch < _inChannels; ch++) {
                  anyOld = anyOld || (_pIsIsolated[ch] == 1U);
               }
               for (size_t ch = 0U; ch < _inChannels; ch++) {
                  setArgs.isMute[ch] = (anyOld && (_pIsIsolated[ch] == 0U)) ? 1U : 0U;
               }
            }
         }
         if (errorCtx.isSuccess()) {
            memcpy(pSetArgs, &setArgs, sizeof(AUDIOLIB_muteNCh_SetArgs));
            _bypassMode = pendingBypassMode;
            if (pNewIsolated != _pIsIsolated) {
               memcpy(_pIsIsolated, pNewIsolated, _inChannels * sizeof(uint32_t));
            }
         }
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool IsolateNCh<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("IsolateNCh::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("IsolateNCh::getParams", "size pointer is null");
   }
   else if (!_setArgsBuf) {
      errorCtx.setMemoryError("IsolateNCh::getParams", "_setArgsBuf is null");
   }
   else {
      auto *pSetArgs = reinterpret_cast<AUDIOLIB_muteNCh_SetArgs *>(_setArgsBuf.get());

      switch (static_cast<IsolateNChParamId>(paramId)) {
      case IsolateNChParamId::kIsIsolated: {
         size_t expectedSize = (_inChannels * sizeof(uint32_t));
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("IsolateNCh::getParams",
                                              "Value size smaller than expected for kIsIsolated");
         }
         else {
            memcpy(value, _pIsIsolated, expectedSize);
         }
         *size = expectedSize;
         break;
      }
      case IsolateNChParamId::kFadeTime: {
         if (*size < sizeof(pSetArgs->fadeTime)) {
            errorCtx.setInvalidParameterError("IsolateNCh::getParams",
                                              "Value size smaller than expected for kFadeTime");
         }
         else {
            memcpy(value, &pSetArgs->fadeTime, sizeof(pSetArgs->fadeTime));
         }
         *size = sizeof(pSetArgs->fadeTime);
         break;
      }
      case IsolateNChParamId::kFadeType: {
         if (*size < sizeof(pSetArgs->fadeType)) {
            errorCtx.setInvalidParameterError("IsolateNCh::getParams",
                                              "Value size smaller than expected for kFadeType");
         }
         else {
            memcpy(value, &pSetArgs->fadeType, sizeof(pSetArgs->fadeType));
         }
         *size = sizeof(pSetArgs->fadeType);
         break;
      }
      case IsolateNChParamId::kBypass: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("IsolateNCh::getParams", "Value size smaller than expected for kBypass");
         }
         else {
            memcpy(value, &_bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case IsolateNChParamId::kAll: {
         IsolateNChTunableParams<T> *pParams = static_cast<IsolateNChTunableParams<T> *>(value);
         if (*size < sizeof(IsolateNChTunableParams<T>)) {
            errorCtx.setInvalidParameterError("IsolateNCh::getParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pIsIsolated == nullptr) {
            errorCtx.setNullPointerError("IsolateNCh::getParams", "isolate pointer pIsIsolated is null");
         }
         else {
            if (pParams->pIsIsolated != _pIsIsolated) {
               memcpy(pParams->pIsIsolated, _pIsIsolated, _inChannels * sizeof(uint32_t));
            }
            pParams->fadeTime   = pSetArgs->fadeTime;
            pParams->fadeType   = pSetArgs->fadeType;
            pParams->bypassMode = _bypassMode;
         }
         *size = sizeof(IsolateNChTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("IsolateNCh::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class IsolateNCh<float>;

} // namespace AUDIOLIB
} // namespace TISP
