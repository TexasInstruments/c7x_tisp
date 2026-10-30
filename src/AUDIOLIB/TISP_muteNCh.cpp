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
Mutench<T>::Mutench(const TISP_BufDescList        *pInputs,
                    const TISP_BufDescList        *pOutputs,
                    const MutenchConfig      *pConfig,
                    const MutenchTunableParams<T> *pDefaultTunableParams,
                    const char                    *nodeName,
                    uint32_t                       nodeId,
                    ErrorCtxt                     &errorCtx)
    : _in{}, _out{}, _inChannels(0U), _samplingRate(0), _bypassMode(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("Mutench constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("Mutench constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pIsMute == nullptr) {
      errorCtx.setNullPointerError("Mutench constructor", "mute pointer pDefaultTunableParams->pIsMute is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Mutench constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Mutench constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("Mutench constructor", "input and output interleave must match");
      }
      else if (pConfig->samplingRate <= 0) {
         errorCtx.setInvalidParameterError("Mutench constructor", "samplingRate must be greater than zero");
      }
      else if (pDefaultTunableParams->bypassMode > 1U) {
         errorCtx.setInvalidParameterError("Mutench constructor", "bypass mode must be either zero or one");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // One mute flag per channel: channels are columns when interleaved, rows otherwise
      _inChannels   = (_in.isInterleave == 1U) ? _in.width : _in.height;
      _samplingRate = pConfig->samplingRate;
      _bypassMode   = pDefaultTunableParams->bypassMode;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numChannels=%zu,samplingRate=%d", _inChannels,
                      _samplingRate);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Mutench from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_muteNCh_InitArgs kerInitArgs;
      kerInitArgs.funcStyle     = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate  = _samplingRate;
      kerInitArgs.isInterleaved = _in.isInterleave;
      kerInitArgs.numChannels   = static_cast<uint32_t>(_inChannels);

      _setArgsBuf = make_unique_handle<char>(sizeof(AUDIOLIB_muteNCh_SetArgs));

      if (!_setArgsBuf) {
         errorCtx.setMemoryError("Mutench constructor", "failed to allocate _setArgsBuf");
      }
      else {
         AUDIOLIB_muteNCh_SetArgs *pSetArgs = reinterpret_cast<AUDIOLIB_muteNCh_SetArgs *>(_setArgsBuf.get());
         pSetArgs->isMute                   = pDefaultTunableParams->pIsMute;
         pSetArgs->fadeTime                 = pDefaultTunableParams->fadeTime;
         pSetArgs->fadeType                 = pDefaultTunableParams->fadeType;

         _handle = make_unique_handle<char>(AUDIOLIB_muteNCh_getHandleSize(&kerInitArgs));

         if (!_handle) {
            errorCtx.setMemoryError("Mutench constructor", "failed to allocate muteNCh handle");
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
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *Mutench<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Mutench<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Mutench<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Mutench setAddr]
template <typename T> void Mutench<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Mutench<T>::bypassCopy() noexcept
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

template <typename T> void Mutench<T>::exec() noexcept
{
   if (_bypassMode != 0U) {
      bypassCopy();
   }
   else {
      AUDIOLIB_muteNCh_exec(_handle.get(), _in.pBuf, _out.pBuf);
   }
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

      /* Bypass is node local state and is not forwarded to the kernel. */
      bool     applyToKernel     = true;
      uint32_t pendingBypassMode = _bypassMode;

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
      case MutenchParamId::kBypass: {
         applyToKernel = false;
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Mutench::setParams", "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode = 0U;
            memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("Mutench::setParams", "bypass mode must be either zero or one");
            }
            else {
               _bypassMode = bypassMode;
            }
         }
         break;
      }
      case MutenchParamId::kAll: {
         const MutenchTunableParams<T> *pParams = static_cast<const MutenchTunableParams<T> *>(value);
         if (size < sizeof(MutenchTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Mutench::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pIsMute == nullptr) {
            errorCtx.setNullPointerError("Mutench::setParams", "mute pointer pIsMute is null");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("Mutench::setParams", "bypass mode must be either zero or one");
         }
         else {
            if (pParams->pIsMute != setArgs.isMute) {
               memcpy(setArgs.isMute, pParams->pIsMute, _inChannels * sizeof(uint32_t));
            }
            setArgs.fadeTime  = pParams->fadeTime;
            setArgs.fadeType  = pParams->fadeType;
            pendingBypassMode = pParams->bypassMode;
            paramName         = "kAll";
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Mutench::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess() && applyToKernel) {
         AUDIOLIB_STATUS status = AUDIOLIB_muteNCh_set(_handle.get(), &setArgs);
         std::string     msg    = std::string("AUDIOLIB_muteNCh_set with ") + paramName;
         (void) errorCtx.remapXLIBError(status, msg.c_str());
         if (errorCtx.isSuccess()) {
            memcpy(pSetArgs, &setArgs, sizeof(AUDIOLIB_muteNCh_SetArgs));
            _bypassMode = pendingBypassMode;
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
      case MutenchParamId::kBypass: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Mutench::getParams", "Value size smaller than expected for kBypass");
         }
         else {
            memcpy(value, &_bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case MutenchParamId::kAll: {
         MutenchTunableParams<T> *pParams = static_cast<MutenchTunableParams<T> *>(value);
         if (*size < sizeof(MutenchTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Mutench::getParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pIsMute == nullptr) {
            errorCtx.setNullPointerError("Mutench::getParams", "mute pointer pIsMute is null");
         }
         else {
            if (pParams->pIsMute != pSetArgs->isMute) {
               memcpy(pParams->pIsMute, pSetArgs->isMute, _inChannels * sizeof(uint32_t));
            }
            pParams->fadeTime   = pSetArgs->fadeTime;
            pParams->fadeType   = pSetArgs->fadeType;
            pParams->bypassMode = _bypassMode;
         }
         *size = sizeof(MutenchTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Mutench::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class Mutench<float>;

} // namespace AUDIOLIB
} // namespace TISP
