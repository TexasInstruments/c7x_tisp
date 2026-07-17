// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_balanceFader.hpp>
#include <cstring>
#include <string>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::BalanceFader constructor]

template <typename T>
BalanceFader<T>::BalanceFader(T         *pX,
                              T         *pG,
                              T         *pY,
                              size_t     widthIn,
                              size_t     heightIn,
                              size_t     gWidthIn,
                              size_t     inPitchBytes,
                              size_t     outPitchBytes,
                              size_t     isInterleave,
                              float      balance,
                              float      fade,
                              int32_t   *channelConfig,
                              float      lfeBalanceGainFactor,
                              float      lfeFaderGainFactor,
                              float      sideGainFactor,
                              int32_t    lfeBalanceMode,
                              int32_t    lfeFaderMode,
                              ErrorCtxt &errorCtx)
    : _pX(pX), _pG(pG), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "widthIn=%zu,heightIn=%zu,gWidthIn=%zu,isInterleave=%zu", widthIn, heightIn, gWidthIn, isInterleave);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("BalanceFader constructor", "input pointer pX is null");
   }
   else if (pG == nullptr) {
      errorCtx.setNullPointerError("BalanceFader constructor", "gain pointer pG is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("BalanceFader constructor", "output pointer pY is null");
   }
   else if (widthIn == 0) {
      errorCtx.setInvalidParameterError("BalanceFader constructor", "width must be greater than zero");
   }
   else if (heightIn == 0) {
      errorCtx.setInvalidParameterError("BalanceFader constructor", "height must be greater than zero");
   }
   else if (gWidthIn == 0) {
      errorCtx.setInvalidParameterError("BalanceFader constructor", "width of gain matrix must be greater than zero");
   }
   else if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("BalanceFader constructor", "interleave must be either zero or one");
   }
   else if (balance < -1 || balance > 1) {
      errorCtx.setInvalidParameterError("BalanceFader constructor", "left/right control must be between -1 and 1");
   }
   else if (fade < -1 || fade > 1) {
      errorCtx.setInvalidParameterError("BalanceFader constructor", "front/rear control must be between -1 and 1");
   }
   else if (channelConfig == nullptr) {
      errorCtx.setInvalidParameterError("BalanceFader constructor", "channel config pointer is null");
   }
   else if (lfeBalanceGainFactor < 0 || lfeBalanceGainFactor > 1) {
      errorCtx.setInvalidParameterError("BalanceFader constructor",
                                        "LFE balance gain factor must be between zero and one");
   }
   else if (lfeFaderGainFactor < 0 || lfeFaderGainFactor > 1) {
      errorCtx.setInvalidParameterError("BalanceFader constructor",
                                        "LFE fader gain factor must be between zero and one");
   }
   else if (sideGainFactor < 0 || sideGainFactor > 1) {
      errorCtx.setInvalidParameterError("BalanceFader constructor", "Side gain factor must be between zero and one");
   }
   else if (lfeBalanceMode < 0 || lfeBalanceMode > 2) {
      errorCtx.setInvalidParameterError(
          "BalanceFader constructor",
          "mode for LFE channel response to balance control must be either zero or one or two");
   }
   else if (lfeFaderMode < 0 || lfeFaderMode > 3) {
      errorCtx.setInvalidParameterError(
          "BalanceFader constructor", "mode for LFE channel response to fader control must be between zero and three");
   }
   else {
      _widthIn              = widthIn;
      _heightIn             = heightIn;
      _gWidthIn             = gWidthIn;
      _inPitchBytes         = inPitchBytes;
      _outPitchBytes        = outPitchBytes;
      _isInterleave         = isInterleave;
      _balance              = balance;
      _fade                 = fade;
      _channelConfig        = channelConfig;
      _lfeBalanceGainFactor = lfeBalanceGainFactor;
      _lfeFaderGainFactor   = lfeFaderGainFactor;
      _sideGainFactor       = sideGainFactor;
      _lfeBalanceMode       = lfeBalanceMode;
      _lfeFaderMode         = lfeFaderMode;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      _nodeInfo.bufParams.widthIn[0]        = widthIn;
      _nodeInfo.bufParams.heightIn[0]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      _nodeInfo.bufParams.widthOut[0]        = widthIn;
      _nodeInfo.bufParams.heightOut[0]       = heightIn;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Balancefader from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_balanceFader_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = isInterleave;

      _setArgsBuf = make_unique_handle<char>(sizeof(AUDIOLIB_balanceFader_SetArgs));
      AUDIOLIB_balanceFader_SetArgs *pSetArgs = nullptr;
      if (!_setArgsBuf) {
         errorCtx.setMemoryError("BalanceFader constructor", "failed to allocate _setArgsBuf");
      }
      else {
         pSetArgs                       = reinterpret_cast<AUDIOLIB_balanceFader_SetArgs *>(_setArgsBuf.get());
         pSetArgs->balance              = balance;
         pSetArgs->fade                 = fade;
         pSetArgs->channelConfig        = channelConfig;
         pSetArgs->lfeBalanceGainFactor = lfeBalanceGainFactor;
         pSetArgs->lfeFaderGainFactor   = lfeFaderGainFactor;
         pSetArgs->sideGainFactor       = sideGainFactor;
         pSetArgs->lfeBalanceMode       = lfeBalanceMode;
         pSetArgs->lfeFaderMode         = lfeFaderMode;
      }

      if (errorCtx.isSuccess()) {
         _handle = make_unique_handle<char>(AUDIOLIB_balanceFader_getHandleSize(&kerInitArgs));

         if (!_handle) {
            errorCtx.setMemoryError("BalanceFader constructor", "failed to allocate balanceFader handle");
         }

         if (errorCtx.isSuccess()) {
            AUDIOLIB_bufParams2D_t bufParamsIn;
            AUDIOLIB_bufParams2D_t bufParamsOut;
            AUDIOLIB_bufParams1D_t bufParamsGain;

            bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsIn.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsIn.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

            bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsOut.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsOut.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);

            bufParamsGain.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsGain.dim_x     = static_cast<uint32_t>(gWidthIn);
            /******************************************************************************/
            /* Validate parameters before init                                            */
            /******************************************************************************/

            AUDIOLIB_STATUS status = AUDIOLIB_balanceFader_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsGain,
                                                                            &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_balanceFader_init_checkParams");

            /******************************************************************************/
            /* Call init function from AUDIOLIB to for balanceFader to set SE/SA params             */
            /******************************************************************************/

            if (errorCtx.isSuccess()) {
               status =
                   AUDIOLIB_balanceFader_init(_handle.get(), &bufParamsIn, &bufParamsGain, &bufParamsOut, &kerInitArgs);
               errorCtx.remapXLIBError(status, "AUDIOLIB_balanceFader_init");
               if (errorCtx.isSuccess()) {
                  status = AUDIOLIB_balanceFader_set(_handle.get(), _pG, pSetArgs);
                  errorCtx.remapXLIBError(status, "AUDIOLIB_balanceFader_set");
               }
            }
         }
      }
   }
}

template <typename T> void *BalanceFader<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *BalanceFader<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t BalanceFader<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t BalanceFader<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Balancefader setAddr]
template <typename T> void BalanceFader<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void BalanceFader<T>::exec() noexcept
{
   AUDIOLIB_balanceFader_exec(_handle.get(), _pX, _pG, _pY);
}

template <typename T>
BalanceFader<T>::BalanceFader(T          *pX,
                              T          *pG,
                              T          *pY,
                              size_t      widthIn,
                              size_t      heightIn,
                              size_t      gWidthIn,
                              size_t      inPitchBytes,
                              size_t      outPitchBytes,
                              size_t      isInterleave,
                              float       balance,
                              float       fade,
                              int32_t    *channelConfig,
                              float       lfeBalanceGainFactor,
                              float       lfeFaderGainFactor,
                              float       sideGainFactor,
                              int32_t     lfeBalanceMode,
                              int32_t     lfeFaderMode,
                              const char *nodeName,
                              uint32_t    nodeId,
                              ErrorCtxt  &errorCtx)
    : BalanceFader(pX,
                   pG,
                   pY,
                   widthIn,
                   heightIn,
                   gWidthIn,
                   inPitchBytes,
                   outPitchBytes,
                   isInterleave,
                   balance,
                   fade,
                   channelConfig,
                   lfeBalanceGainFactor,
                   lfeFaderGainFactor,
                   sideGainFactor,
                   lfeBalanceMode,
                   lfeFaderMode,
                   errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T>
bool BalanceFader<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("BalanceFader::setParams", "value pointer is null");
   }
   else if (!_setArgsBuf) {
      errorCtx.setMemoryError("BalanceFader::setParams", "_setArgsBuf is null");
   }
   else {
      auto                         *pSetArgs = reinterpret_cast<AUDIOLIB_balanceFader_SetArgs *>(_setArgsBuf.get());
      AUDIOLIB_balanceFader_SetArgs setArgs  = {0};
      memcpy(&setArgs, pSetArgs, sizeof(AUDIOLIB_balanceFader_SetArgs));

      const char *paramName = "Unknown";

      switch (static_cast<BalanceFaderParamId>(paramId)) {
      case BalanceFaderParamId::kBalance: {
         if (size < sizeof(setArgs.balance)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kBalance");
         }
         else {
            memcpy(&setArgs.balance, value, sizeof(setArgs.balance));
            paramName = "kBalance";
         }
         break;
      }
      case BalanceFaderParamId::kFade: {
         if (size < sizeof(setArgs.fade)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams", "Value size smaller than expected for kFade");
         }
         else {
            memcpy(&setArgs.fade, value, sizeof(setArgs.fade));
            paramName = "kFade";
         }
         break;
      }
      case BalanceFaderParamId::kLfeBalanceGainFactor: {
         if (size < sizeof(setArgs.lfeBalanceGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kLfeBalanceGainFactor");
         }
         else {
            memcpy(&setArgs.lfeBalanceGainFactor, value, sizeof(setArgs.lfeBalanceGainFactor));
            paramName = "kLfeBalanceGainFactor";
         }
         break;
      }
      case BalanceFaderParamId::kLfeFaderGainFactor: {
         if (size < sizeof(setArgs.lfeFaderGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kLfeFaderGainFactor");
         }
         else {
            memcpy(&setArgs.lfeFaderGainFactor, value, sizeof(setArgs.lfeFaderGainFactor));
            paramName = "kLfeFaderGainFactor";
         }
         break;
      }
      case BalanceFaderParamId::kSideGainFactor: {
         if (size < sizeof(setArgs.sideGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kSideGainFactor");
         }
         else {
            memcpy(&setArgs.sideGainFactor, value, sizeof(setArgs.sideGainFactor));
            paramName = "kSideGainFactor";
         }
         break;
      }
      case BalanceFaderParamId::kLfeBalanceMode: {
         if (size < sizeof(setArgs.lfeBalanceMode)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kLfeBalanceMode");
         }
         else {
            memcpy(&setArgs.lfeBalanceMode, value, sizeof(setArgs.lfeBalanceMode));
            paramName = "kLfeBalanceMode";
         }
         break;
      }
      case BalanceFaderParamId::kLfeFaderMode: {
         if (size < sizeof(setArgs.lfeFaderMode)) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kLfeFaderMode");
         }
         else {
            memcpy(&setArgs.lfeFaderMode, value, sizeof(setArgs.lfeFaderMode));
            paramName = "kLfeFaderMode";
         }
         break;
      }
      case BalanceFaderParamId::kChannelConfig: {
         size_t expectedSize = (_gWidthIn * sizeof(int32_t));
         if (_channelConfig == nullptr) {
            errorCtx.setNullPointerError("BalanceFader::setParams", "_channelConfig is null");
         }
         else if (size < expectedSize) {
            errorCtx.setInvalidParameterError("BalanceFader::setParams",
                                              "Value size smaller than expected for kChannelConfig");
         }
         else {
            memcpy(_channelConfig, value, expectedSize);
            setArgs.channelConfig = _channelConfig;
            paramName             = "kChannelConfig";
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("BalanceFader::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_STATUS status = AUDIOLIB_balanceFader_set(_handle.get(), _pG, &setArgs);
         std::string     msg    = std::string("AUDIOLIB_balanceFader_set with ") + paramName;
         (void) errorCtx.remapXLIBError(status, msg.c_str());
         if (errorCtx.isSuccess()) {
            memcpy(pSetArgs, &setArgs, sizeof(AUDIOLIB_balanceFader_SetArgs));
         }
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool BalanceFader<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("BalanceFader::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("BalanceFader::getParams", "size pointer is null");
   }
   else if (!_setArgsBuf) {
      errorCtx.setMemoryError("BalanceFader::getParams", "_setArgsBuf is null");
   }
   else {
      auto *pSetArgs = reinterpret_cast<AUDIOLIB_balanceFader_SetArgs *>(_setArgsBuf.get());

      switch (static_cast<BalanceFaderParamId>(paramId)) {
      case BalanceFaderParamId::kBalance: {
         if (*size < sizeof(pSetArgs->balance)) {
            errorCtx.setInvalidParameterError("BalanceFader::getParams",
                                              "Value size smaller than expected for kBalance");
         }
         else {
            memcpy(value, &pSetArgs->balance, sizeof(pSetArgs->balance));
         }
         *size = sizeof(pSetArgs->balance);
         break;
      }
      case BalanceFaderParamId::kFade: {
         if (*size < sizeof(pSetArgs->fade)) {
            errorCtx.setInvalidParameterError("BalanceFader::getParams", "Value size smaller than expected for kFade");
         }
         else {
            memcpy(value, &pSetArgs->fade, sizeof(pSetArgs->fade));
         }
         *size = sizeof(pSetArgs->fade);
         break;
      }
      case BalanceFaderParamId::kLfeBalanceGainFactor: {
         if (*size < sizeof(pSetArgs->lfeBalanceGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::getParams",
                                              "Value size smaller than expected for kLfeBalanceGainFactor");
         }
         else {
            memcpy(value, &pSetArgs->lfeBalanceGainFactor, sizeof(pSetArgs->lfeBalanceGainFactor));
         }
         *size = sizeof(pSetArgs->lfeBalanceGainFactor);
         break;
      }
      case BalanceFaderParamId::kLfeFaderGainFactor: {
         if (*size < sizeof(pSetArgs->lfeFaderGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::getParams",
                                              "Value size smaller than expected for kLfeFaderGainFactor");
         }
         else {
            memcpy(value, &pSetArgs->lfeFaderGainFactor, sizeof(pSetArgs->lfeFaderGainFactor));
         }
         *size = sizeof(pSetArgs->lfeFaderGainFactor);
         break;
      }
      case BalanceFaderParamId::kSideGainFactor: {
         if (*size < sizeof(pSetArgs->sideGainFactor)) {
            errorCtx.setInvalidParameterError("BalanceFader::getParams",
                                              "Value size smaller than expected for kSideGainFactor");
         }
         else {
            memcpy(value, &pSetArgs->sideGainFactor, sizeof(pSetArgs->sideGainFactor));
         }
         *size = sizeof(pSetArgs->sideGainFactor);
         break;
      }
      case BalanceFaderParamId::kLfeBalanceMode: {
         if (*size < sizeof(pSetArgs->lfeBalanceMode)) {
            errorCtx.setInvalidParameterError("BalanceFader::getParams",
                                              "Value size smaller than expected for kLfeBalanceMode");
         }
         else {
            memcpy(value, &pSetArgs->lfeBalanceMode, sizeof(pSetArgs->lfeBalanceMode));
         }
         *size = sizeof(pSetArgs->lfeBalanceMode);
         break;
      }
      case BalanceFaderParamId::kLfeFaderMode: {
         if (*size < sizeof(pSetArgs->lfeFaderMode)) {
            errorCtx.setInvalidParameterError("BalanceFader::getParams",
                                              "Value size smaller than expected for kLfeFaderMode");
         }
         else {
            memcpy(value, &pSetArgs->lfeFaderMode, sizeof(pSetArgs->lfeFaderMode));
         }
         *size = sizeof(pSetArgs->lfeFaderMode);
         break;
      }
      case BalanceFaderParamId::kChannelConfig: {
         size_t expectedSize = (_gWidthIn * sizeof(int32_t));
         if (_channelConfig == nullptr) {
            errorCtx.setNullPointerError("BalanceFader::getParams", "_channelConfig is null");
         }
         else if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("BalanceFader::getParams",
                                              "Value size smaller than expected for kChannelConfig");
         }
         else {
            memcpy(value, _channelConfig, expectedSize);
         }
         *size = expectedSize;
         break;
      }
      default:
         errorCtx.setInvalidParameterError("BalanceFader::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class BalanceFader<float>;

} // namespace AUDIOLIB
} // namespace TISP
