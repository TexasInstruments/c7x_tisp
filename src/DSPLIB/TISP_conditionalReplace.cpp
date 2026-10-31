// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_conditionalReplace.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename T>
ConditionalReplace<T>::ConditionalReplace(
    const TISP_BufDescList                        *pInputs,
    const TISP_BufDescList                        *pOutputs,
    const ConditionalReplaceTunableParams<T> *pDefaultTunableParams,
    const char                                    *nodeName,
    uint32_t                                       nodeId,
    ErrorCtxt                                     &errorCtx)
    : _in{}, _out{}, _compareType(nullptr), _constValue(nullptr), _replaceValue(nullptr)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("ConditionalReplace constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pCompareType == nullptr) {
      errorCtx.setNullPointerError("ConditionalReplace constructor",
                                   "pointer pDefaultTunableParams->pCompareType is null");
   }
   else if (pDefaultTunableParams->pConstValue == nullptr) {
      errorCtx.setNullPointerError("ConditionalReplace constructor",
                                   "pointer pDefaultTunableParams->pConstValue is null");
   }
   else if (pDefaultTunableParams->pReplaceValue == nullptr) {
      errorCtx.setNullPointerError("ConditionalReplace constructor",
                                   "pointer pDefaultTunableParams->pReplaceValue is null");
   }
   else {
      /* Nothing to do here */
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "ConditionalReplace constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "ConditionalReplace constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("ConditionalReplace constructor", "input 0 must be planar");
      }
      else if (_out.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("ConditionalReplace constructor", "output 0 must be planar");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _compareType  = pDefaultTunableParams->pCompareType;
      _constValue   = pDefaultTunableParams->pConstValue;
      _replaceValue = pDefaultTunableParams->pReplaceValue;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,isInterleave=%u",
                      _in.width, _in.height, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      DSPLIB_conditionalReplace_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_conditionalReplace_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("ConditionalReplace constructor",
                                 "failed to allocate conditionalReplace handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn  = TISP_toDsplibBufParams2D<T>(_in);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<T>(_out);
         DSPLIB_STATUS        status =
             DSPLIB_conditionalReplace_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_conditionalReplace_init_checkParams")) {
            status = DSPLIB_conditionalReplace_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "DSPLIB_conditionalReplace_init")) {
               DSPLIB_conditionalReplace_SetArgs kerSetArgs;
               kerSetArgs.pCompareType  = _compareType;
               kerSetArgs.pConstValue   = _constValue;
               kerSetArgs.pReplaceValue = _replaceValue;
               status                   = DSPLIB_conditionalReplace_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(status, "DSPLIB_conditionalReplace_set");
            }
         }
      }
   }
}

template <typename T> void *ConditionalReplace<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t ConditionalReplace<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *ConditionalReplace<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t ConditionalReplace<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void ConditionalReplace<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void ConditionalReplace<T>::exec() noexcept
{
   DSPLIB_conditionalReplace_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template <typename T>
bool ConditionalReplace<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("ConditionalReplace::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<ConditionalReplaceParamId>(paramId)) {
      case ConditionalReplaceParamId::kCompareType: {
         size_t expectedSize = ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("ConditionalReplace::setParams",
                                              "Value size smaller than expected for kCompareType");
         }
         else {
            memcpy(_compareType, value, expectedSize);
         }
         break;
      }
      case ConditionalReplaceParamId::kConstValue: {
         size_t expectedSize = ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("ConditionalReplace::setParams",
                                              "Value size smaller than expected for kConstValue");
         }
         else {
            memcpy(_constValue, value, expectedSize);
         }
         break;
      }
      case ConditionalReplaceParamId::kReplaceValue: {
         size_t expectedSize = ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("ConditionalReplace::setParams",
                                              "Value size smaller than expected for kReplaceValue");
         }
         else {
            memcpy(_replaceValue, value, expectedSize);
         }
         break;
      }
      case ConditionalReplaceParamId::kAll: {
         const ConditionalReplaceTunableParams<T> *pParams =
             static_cast<const ConditionalReplaceTunableParams<T> *>(value);
         if (size < sizeof(ConditionalReplaceTunableParams<T>)) {
            errorCtx.setInvalidParameterError("ConditionalReplace::setParams",
                                              "Value size smaller than expected for kAll");
         }
         else if (pParams->pCompareType == nullptr) {
            errorCtx.setNullPointerError("ConditionalReplace::setParams", "pointer pCompareType is null");
         }
         else if (pParams->pConstValue == nullptr) {
            errorCtx.setNullPointerError("ConditionalReplace::setParams", "pointer pConstValue is null");
         }
         else if (pParams->pReplaceValue == nullptr) {
            errorCtx.setNullPointerError("ConditionalReplace::setParams", "pointer pReplaceValue is null");
         }
         else {
            if (pParams->pCompareType != _compareType) {
               memcpy(_compareType, pParams->pCompareType,
                      ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t));
            }
            if (pParams->pConstValue != _constValue) {
               memcpy(_constValue, pParams->pConstValue,
                      ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t));
            }
            if (pParams->pReplaceValue != _replaceValue) {
               memcpy(_replaceValue, pParams->pReplaceValue,
                      ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t));
            }
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("ConditionalReplace::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_conditionalReplace_SetArgs kerSetArgs;
         kerSetArgs.pCompareType    = _compareType;
         kerSetArgs.pConstValue     = _constValue;
         kerSetArgs.pReplaceValue   = _replaceValue;
         const DSPLIB_STATUS status = DSPLIB_conditionalReplace_set(_handle.get(), &kerSetArgs);
         errorCtx.remapXLIBError(status, "DSPLIB_conditionalReplace_set");
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool ConditionalReplace<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("ConditionalReplace::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("ConditionalReplace::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<ConditionalReplaceParamId>(paramId)) {
      case ConditionalReplaceParamId::kCompareType: {
         size_t expectedSize = ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t);
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("ConditionalReplace::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, _compareType, expectedSize);
         }
         *size = expectedSize;
         break;
      }
      case ConditionalReplaceParamId::kConstValue: {
         size_t expectedSize = ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t);
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("ConditionalReplace::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, _constValue, expectedSize);
         }
         *size = expectedSize;
         break;
      }
      case ConditionalReplaceParamId::kReplaceValue: {
         size_t expectedSize = ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t);
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("ConditionalReplace::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, _replaceValue, expectedSize);
         }
         *size = expectedSize;
         break;
      }
      case ConditionalReplaceParamId::kAll: {
         ConditionalReplaceTunableParams<T> *pParams =
             static_cast<ConditionalReplaceTunableParams<T> *>(value);
         if (*size < sizeof(ConditionalReplaceTunableParams<T>)) {
            errorCtx.setInvalidParameterError("ConditionalReplace::getParams", "Value size smaller than expected");
         }
         else if (pParams->pCompareType == nullptr) {
            errorCtx.setNullPointerError("ConditionalReplace::getParams", "pointer pCompareType is null");
         }
         else if (pParams->pConstValue == nullptr) {
            errorCtx.setNullPointerError("ConditionalReplace::getParams", "pointer pConstValue is null");
         }
         else if (pParams->pReplaceValue == nullptr) {
            errorCtx.setNullPointerError("ConditionalReplace::getParams", "pointer pReplaceValue is null");
         }
         else {
            if (pParams->pCompareType != _compareType) {
               memcpy(pParams->pCompareType, _compareType,
                      ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t));
            }
            if (pParams->pConstValue != _constValue) {
               memcpy(pParams->pConstValue, _constValue,
                      ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t));
            }
            if (pParams->pReplaceValue != _replaceValue) {
               memcpy(pParams->pReplaceValue, _replaceValue,
                      ((_in.isInterleave == 1U) ? _in.width : _in.height) * sizeof(int32_t));
            }
         }
         *size = sizeof(ConditionalReplaceTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("ConditionalReplace::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class ConditionalReplace<int32_t>;

} // namespace DSPLIB
} // namespace TISP
