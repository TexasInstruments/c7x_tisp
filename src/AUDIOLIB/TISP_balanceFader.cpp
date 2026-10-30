// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <TISP_balanceFader.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::BalanceFader constructor]

template <typename T>
BalanceFader<T>::BalanceFader(const TISP_BufDescList             *pInputs,
                              const TISP_BufDescList             *pOutputs,
                              const TISP_BufDescList             *pStates,
                              const BalanceFaderTunableParams<T> *pDefaultTunableParams,
                              const char                         *nodeName,
                              uint32_t                            nodeId,
                              ErrorCtxt                          &errorCtx)
    : _in{}, _out{}, _gains(nullptr), _numChannels(0U), _params{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("BalanceFader constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pChannelConfig == nullptr) {
      errorCtx.setNullPointerError("BalanceFader constructor",
                                   "channel config pointer pDefaultTunableParams->pChannelConfig is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output, and one gain state buffer
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "BalanceFader constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "BalanceFader constructor (outputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pStates, 1U, 1U, "BalanceFader constructor (states)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("BalanceFader constructor", "input and output interleave must match");
      }
      else {
         (void) checkTunableParams(*pDefaultTunableParams, "BalanceFader constructor", errorCtx);
      }
   }

   if (errorCtx.isSuccess()) {
      const TISP_BufDesc2D &gainDesc = pStates->pDesc[0];

      // One gain per channel: channels are columns when interleaved, rows otherwise
      _numChannels = (_in.isInterleave == 1U) ? _in.width : _in.height;
      _gains       = static_cast<T *>(gainDesc.pBuf);

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "width=%zu, height=%zu, numChannels=%zu, isInterleave=%u", _in.width, _in.height, _numChannels,
                      static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /****************************************************************************/
      /* Initialize data structures required by AUDIOLIB_balanceFader from AUDIOLIB */
      /****************************************************************************/

      AUDIOLIB_balanceFader_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = _in.isInterleave;

      _handle = make_unique_handle<char>(AUDIOLIB_balanceFader_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("BalanceFader constructor", "failed to allocate balanceFader handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);
         AUDIOLIB_bufParams1D_t bufParamsGain;

         bufParamsGain.data_type = static_cast<uint32_t>(AUDIOLIB_element_dType<T>::value);
         bufParamsGain.dim_x     = static_cast<uint32_t>(gainDesc.width);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status = AUDIOLIB_balanceFader_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsGain,
                                                                         &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for balanceFader and compute the gains    */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "AUDIOLIB_balanceFader_init_checkParams")) {
            status = AUDIOLIB_balanceFader_init(_handle.get(), &bufParamsIn, &bufParamsGain, &bufParamsOut,
                                                &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "AUDIOLIB_balanceFader_init")) {
               applyTunableParams(*pDefaultTunableParams, errorCtx);
            }
         }
      }
   }
}

template <typename T>
bool BalanceFader<T>::checkTunableParams(const BalanceFaderTunableParams<T> &params,
                                         const char                         *funcName,
                                         ErrorCtxt                          &errorCtx) noexcept
{
   if (params.pChannelConfig == nullptr) {
      errorCtx.setNullPointerError(funcName, "channel config pointer pChannelConfig is null");
   }
   else if ((params.balance < -1.0F) || (params.balance > 1.0F)) {
      errorCtx.setInvalidParameterError(funcName, "left/right control must be between -1 and 1");
   }
   else if ((params.fade < -1.0F) || (params.fade > 1.0F)) {
      errorCtx.setInvalidParameterError(funcName, "front/rear control must be between -1 and 1");
   }
   else if ((params.lfeBalanceGainFactor < 0.0F) || (params.lfeBalanceGainFactor > 1.0F)) {
      errorCtx.setInvalidParameterError(funcName, "LFE balance gain factor must be between zero and one");
   }
   else if ((params.lfeFaderGainFactor < 0.0F) || (params.lfeFaderGainFactor > 1.0F)) {
      errorCtx.setInvalidParameterError(funcName, "LFE fader gain factor must be between zero and one");
   }
   else if ((params.sideGainFactor < 0.0F) || (params.sideGainFactor > 1.0F)) {
      errorCtx.setInvalidParameterError(funcName, "Side gain factor must be between zero and one");
   }
   else if ((params.lfeBalanceMode < 0) || (params.lfeBalanceMode > 2)) {
      errorCtx.setInvalidParameterError(funcName,
                                        "mode for LFE channel response to balance control must be between zero and two");
   }
   else if ((params.lfeFaderMode < 0) || (params.lfeFaderMode > 3)) {
      errorCtx.setInvalidParameterError(funcName,
                                        "mode for LFE channel response to fader control must be between zero and three");
   }
   else if (params.bypassMode > 1U) {
      errorCtx.setInvalidParameterError(funcName, "bypass mode must be either zero or one");
   }
   else {
      /* Nothing to do here */
   }

   return errorCtx.isSuccess();
}

template <typename T>
void BalanceFader<T>::applyTunableParams(const BalanceFaderTunableParams<T> &params, ErrorCtxt &errorCtx) noexcept
{
   AUDIOLIB_balanceFader_SetArgs setArgs;
   setArgs.balance              = params.balance;
   setArgs.fade                 = params.fade;
   setArgs.channelConfig        = params.pChannelConfig;
   setArgs.lfeBalanceGainFactor = params.lfeBalanceGainFactor;
   setArgs.lfeFaderGainFactor   = params.lfeFaderGainFactor;
   setArgs.sideGainFactor       = params.sideGainFactor;
   setArgs.lfeBalanceMode       = params.lfeBalanceMode;
   setArgs.lfeFaderMode         = params.lfeFaderMode;

   AUDIOLIB_STATUS status = AUDIOLIB_balanceFader_set(_handle.get(), _gains, &setArgs);

   if (errorCtx.remapXLIBError(status, "AUDIOLIB_balanceFader_set")) {
      _params = params;
   }
}

template <typename T> void *BalanceFader<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *BalanceFader<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t BalanceFader<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t BalanceFader<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Balancefader setAddr]
template <typename T> void BalanceFader<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addr.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void BalanceFader<T>::bypassCopy() noexcept
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

template <typename T> void BalanceFader<T>::exec() noexcept
{
   if (_params.bypassMode != 0U) {
      bypassCopy();
   }
   else {
      AUDIOLIB_balanceFader_exec(_handle.get(), _in.pBuf, _gains, _out.pBuf);
   }
}

template <typename T>
bool BalanceFader<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   BalanceFaderTunableParams<T> params = _params;

   if (value == nullptr) {
      errorCtx.setNullPointerError("BalanceFader::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<BalanceFaderParamId>(paramId)) {
      case BalanceFaderParamId::kBalance: {
         if (size < sizeof(params.balance)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kBalance");
         }
         else {
            (void) memcpy(&params.balance, value, sizeof(params.balance));
         }
         break;
      }
      case BalanceFaderParamId::kFade: {
         if (size < sizeof(params.fade)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams", "Value size smaller than expected for kFade");
         }
         else {
            (void) memcpy(&params.fade, value, sizeof(params.fade));
         }
         break;
      }
      case BalanceFaderParamId::kLfeBalanceGainFactor: {
         if (size < sizeof(params.lfeBalanceGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kLfeBalanceGainFactor");
         }
         else {
            (void) memcpy(&params.lfeBalanceGainFactor, value, sizeof(params.lfeBalanceGainFactor));
         }
         break;
      }
      case BalanceFaderParamId::kLfeFaderGainFactor: {
         if (size < sizeof(params.lfeFaderGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kLfeFaderGainFactor");
         }
         else {
            (void) memcpy(&params.lfeFaderGainFactor, value, sizeof(params.lfeFaderGainFactor));
         }
         break;
      }
      case BalanceFaderParamId::kSideGainFactor: {
         if (size < sizeof(params.sideGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kSideGainFactor");
         }
         else {
            (void) memcpy(&params.sideGainFactor, value, sizeof(params.sideGainFactor));
         }
         break;
      }
      case BalanceFaderParamId::kLfeBalanceMode: {
         if (size < sizeof(params.lfeBalanceMode)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kLfeBalanceMode");
         }
         else {
            (void) memcpy(&params.lfeBalanceMode, value, sizeof(params.lfeBalanceMode));
         }
         break;
      }
      case BalanceFaderParamId::kLfeFaderMode: {
         if (size < sizeof(params.lfeFaderMode)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kLfeFaderMode");
         }
         else {
            (void) memcpy(&params.lfeFaderMode, value, sizeof(params.lfeFaderMode));
         }
         break;
      }
      case BalanceFaderParamId::kChannelConfig: {
         size_t expectedSize = _numChannels * sizeof(int32_t);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kChannelConfig");
         }
         else if (value != _params.pChannelConfig) {
            (void) memcpy(_params.pChannelConfig, value, expectedSize);
         }
         else {
            /* Nothing to do here */
         }
         break;
      }
      case BalanceFaderParamId::kBypass: {
         if (size < sizeof(params.bypassMode)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams", "Value size smaller than expected for kBypass");
         }
         else {
            (void) memcpy(&params.bypassMode, value, sizeof(params.bypassMode));
         }
         break;
      }
      case BalanceFaderParamId::kAll: {
         const BalanceFaderTunableParams<T> *pParams = static_cast<const BalanceFaderTunableParams<T> *>(value);
         if (size < sizeof(BalanceFaderTunableParams<T>)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams", "Value size smaller than expected for kAll");
         }
         else if (checkTunableParams(*pParams, "BalanceFader::setParams", errorCtx)) {
            if (pParams->pChannelConfig != _params.pChannelConfig) {
               (void) memcpy(_params.pChannelConfig, pParams->pChannelConfig, _numChannels * sizeof(int32_t));
            }
            params                = *pParams;
            params.pChannelConfig = _params.pChannelConfig;
         }
         else {
            /* Nothing to do here */
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("BalanceFader::setParams", "Invalid paramId");
         break;
      }
   }

   if (errorCtx.isSuccess() && checkTunableParams(params, "BalanceFader::setParams", errorCtx)) {
      if (static_cast<BalanceFaderParamId>(paramId) == BalanceFaderParamId::kBypass) {
         _params.bypassMode = params.bypassMode;
      }
      else {
         applyTunableParams(params, errorCtx);
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool BalanceFader<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   size_t      expectedSize = 0U;
   const void *pSrc         = nullptr;

   if (size == nullptr) {
      errorCtx.setNullPointerError("BalanceFader::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<BalanceFaderParamId>(paramId)) {
      case BalanceFaderParamId::kBalance:
         expectedSize = sizeof(_params.balance);
         pSrc         = &_params.balance;
         break;
      case BalanceFaderParamId::kFade:
         expectedSize = sizeof(_params.fade);
         pSrc         = &_params.fade;
         break;
      case BalanceFaderParamId::kLfeBalanceGainFactor:
         expectedSize = sizeof(_params.lfeBalanceGainFactor);
         pSrc         = &_params.lfeBalanceGainFactor;
         break;
      case BalanceFaderParamId::kLfeFaderGainFactor:
         expectedSize = sizeof(_params.lfeFaderGainFactor);
         pSrc         = &_params.lfeFaderGainFactor;
         break;
      case BalanceFaderParamId::kSideGainFactor:
         expectedSize = sizeof(_params.sideGainFactor);
         pSrc         = &_params.sideGainFactor;
         break;
      case BalanceFaderParamId::kLfeBalanceMode:
         expectedSize = sizeof(_params.lfeBalanceMode);
         pSrc         = &_params.lfeBalanceMode;
         break;
      case BalanceFaderParamId::kLfeFaderMode:
         expectedSize = sizeof(_params.lfeFaderMode);
         pSrc         = &_params.lfeFaderMode;
         break;
      case BalanceFaderParamId::kChannelConfig:
         expectedSize = _numChannels * sizeof(int32_t);
         pSrc         = _params.pChannelConfig;
         break;
      case BalanceFaderParamId::kBypass:
         expectedSize = sizeof(_params.bypassMode);
         pSrc         = &_params.bypassMode;
         break;
      case BalanceFaderParamId::kAll:
         expectedSize = sizeof(BalanceFaderTunableParams<T>);
         break;
      default:
         errorCtx.setInvalidParameterError("BalanceFader::getParams", "Invalid paramId");
         break;
      }
   }

   if (errorCtx.isSuccess()) {
      if (value == nullptr) {
         errorCtx.setNullPointerError("BalanceFader::getParams", "value pointer is null");
      }
      else if (*size < expectedSize) {
         errorCtx.setInvalidParameterError("BalanceFader::getParams", "Provided buffer size is too small");
      }
      else if (static_cast<BalanceFaderParamId>(paramId) == BalanceFaderParamId::kAll) {
         BalanceFaderTunableParams<T> *pParams = static_cast<BalanceFaderTunableParams<T> *>(value);
         if (pParams->pChannelConfig == nullptr) {
            errorCtx.setNullPointerError("BalanceFader::getParams", "channel config pointer pChannelConfig is null");
         }
         else {
            int32_t *pChannelConfig = pParams->pChannelConfig;
            if (pChannelConfig != _params.pChannelConfig) {
               (void) memcpy(pChannelConfig, _params.pChannelConfig, _numChannels * sizeof(int32_t));
            }
            *pParams                = _params;
            pParams->pChannelConfig = pChannelConfig;
         }
      }
      else if (value != pSrc) {
         (void) memcpy(value, pSrc, expectedSize);
      }
      else {
         /* Nothing to do here */
      }
      *size = expectedSize;
   }

   return errorCtx.isSuccess();
}

template class BalanceFader<float>;

} // namespace AUDIOLIB
} // namespace TISP
