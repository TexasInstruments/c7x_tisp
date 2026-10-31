// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_binaryLogicGate.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename T>
BinaryLogicGate<T>::BinaryLogicGate(const TISP_BufDescList                *pInputs,
                                    const TISP_BufDescList                *pOutputs,
                                    const BinaryLogicGateTunableParams<T> *pDefaultTunableParams,
                                    const char                            *nodeName,
                                    uint32_t                               nodeId,
                                    ErrorCtxt                             &errorCtx)
    : _in{}, _inputs{}, _out{}, _opType(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("BinaryLogicGate constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else {
      /* Nothing to do here */
   }

   if (TISP_checkBufDescList<T>(pInputs, 2U, 2U, "BinaryLogicGate constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "BinaryLogicGate constructor (outputs)", errorCtx)) {
      for (size_t i = 0U; i < 2U; i++) {
         _in[i]     = pInputs->pDesc[i]; // NOLINT(MISRA.PTR.ARITH)
         _inputs[i] = _in[i].pBuf;
      }
      _out = pOutputs->pDesc[0];

      if ((_in[0].isInterleave != _out.isInterleave) || (_in[1].isInterleave != _out.isInterleave)) {
         errorCtx.setInvalidParameterError("BinaryLogicGate constructor", "input and output interleave must match");
      }
      else if (pDefaultTunableParams->opType > 2U) {
         errorCtx.setInvalidParameterError("BinaryLogicGate constructor", "opType is out of range");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _opType = pDefaultTunableParams->opType;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,isInterleave=%u",
                      _in[0].width, _in[0].height, static_cast<uint32_t>(_in[0].isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      DSPLIB_binaryLogicGate_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_binaryLogicGate_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("BinaryLogicGate constructor", "failed to allocate binaryLogicGate handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn0 = TISP_toDsplibBufParams2D<T>(_in[0]);
         DSPLIB_bufParams2D_t bufParamsIn1 = TISP_toDsplibBufParams2D<T>(_in[1]);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<T>(_out);
         DSPLIB_STATUS status = DSPLIB_binaryLogicGate_init_checkParams(_handle.get(), &bufParamsIn0, &bufParamsIn1,
                                                                        &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_binaryLogicGate_init_checkParams")) {
            status =
                DSPLIB_binaryLogicGate_init(_handle.get(), &bufParamsIn0, &bufParamsIn1, &bufParamsOut, &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "DSPLIB_binaryLogicGate_init")) {
               DSPLIB_binaryLogicGate_SetArgs kerSetArgs;
               kerSetArgs.opType = _opType;
               status            = DSPLIB_binaryLogicGate_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(status, "DSPLIB_binaryLogicGate_set");
            }
         }
      }
   }
}

template <typename T> void *BinaryLogicGate<T>::getInputAddr(size_t index) const noexcept
{
   return (index < 2U) ? _inputs[index] : nullptr;
}

template <typename T> size_t BinaryLogicGate<T>::getNumInputs() const noexcept { return 2U; }

template <typename T> void *BinaryLogicGate<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t BinaryLogicGate<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void BinaryLogicGate<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      for (size_t i = 0U; i < 2U; i++) {
         _in[i].pBuf = addrs.src[i];
         _inputs[i]  = addrs.src[i];
      }
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void BinaryLogicGate<T>::exec() noexcept
{
   DSPLIB_binaryLogicGate_exec(_handle.get(), _in[0].pBuf, _in[1].pBuf, _out.pBuf);
}

template <typename T>
bool BinaryLogicGate<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("BinaryLogicGate::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<BinaryLogicGateParamId>(paramId)) {
      case BinaryLogicGateParamId::kOpType: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("BinaryLogicGate::setParams",
                                              "Value size smaller than expected for kOpType");
         }
         else {
            uint32_t opType;
            memcpy(&opType, value, sizeof(uint32_t));
            if (opType > 2U) {
               errorCtx.setInvalidParameterError("BinaryLogicGate::setParams", "opType is out of range");
            }
            else {
               _opType = opType;
            }
         }
         break;
      }
      case BinaryLogicGateParamId::kAll: {
         const BinaryLogicGateTunableParams<T> *pParams = static_cast<const BinaryLogicGateTunableParams<T> *>(value);
         if (size < sizeof(BinaryLogicGateTunableParams<T>)) {
            errorCtx.setInvalidParameterError("BinaryLogicGate::setParams",
                                              "Value size smaller than expected for kAll");
         }
         else if (pParams->opType > 2U) {
            errorCtx.setInvalidParameterError("BinaryLogicGate::setParams", "opType is out of range");
         }
         else {
            _opType = pParams->opType;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("BinaryLogicGate::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_binaryLogicGate_SetArgs kerSetArgs;
         kerSetArgs.opType          = _opType;
         const DSPLIB_STATUS status = DSPLIB_binaryLogicGate_set(_handle.get(), &kerSetArgs);
         errorCtx.remapXLIBError(status, "DSPLIB_binaryLogicGate_set");
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool BinaryLogicGate<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("BinaryLogicGate::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("BinaryLogicGate::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<BinaryLogicGateParamId>(paramId)) {
      case BinaryLogicGateParamId::kOpType: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("BinaryLogicGate::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_opType, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case BinaryLogicGateParamId::kAll: {
         BinaryLogicGateTunableParams<T> *pParams = static_cast<BinaryLogicGateTunableParams<T> *>(value);
         if (*size < sizeof(BinaryLogicGateTunableParams<T>)) {
            errorCtx.setInvalidParameterError("BinaryLogicGate::getParams", "Value size smaller than expected");
         }
         else {
            pParams->opType = _opType;
         }
         *size = sizeof(BinaryLogicGateTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("BinaryLogicGate::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class BinaryLogicGate<uint8_t>;
template class BinaryLogicGate<uint16_t>;
template class BinaryLogicGate<uint32_t>;
template class BinaryLogicGate<uint64_t>;

} // namespace DSPLIB
} // namespace TISP
