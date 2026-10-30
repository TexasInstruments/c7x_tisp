// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_logicalComparator.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename Tin, typename Tout>
LogicalComparator<Tin, Tout>::LogicalComparator(const TISP_BufDescList                    *pInputs,
                                                const TISP_BufDescList                    *pOutputs,
                                                const LogicalComparatorTunableParams<Tin> *pDefaultTunableParams,
                                                const char                                *nodeName,
                                                uint32_t                                   nodeId,
                                                ErrorCtxt                                 &errorCtx)
    : _in{}, _inputs{}, _out{}, _opType(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("LogicalComparator constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else {
      /* Nothing to do here */
   }

   if (TISP_checkBufDescList<Tin>(pInputs, 2U, 2U, "LogicalComparator constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<Tout>(pOutputs, 1U, 1U, "LogicalComparator constructor (outputs)", errorCtx)) {
      for (size_t i = 0U; i < 2U; i++) {
         _in[i]     = pInputs->pDesc[i]; // NOLINT(MISRA.PTR.ARITH)
         _inputs[i] = _in[i].pBuf;
      }
      _out = pOutputs->pDesc[0];

      if ((_in[0].isInterleave != _out.isInterleave) || (_in[1].isInterleave != _in[0].isInterleave)) {
         errorCtx.setInvalidParameterError("LogicalComparator constructor", "input and output interleave must match");
      }
      // The kernel reads both inputs with the row pitch of input 0
      else if (_in[1].pitchBytes != _in[0].pitchBytes) {
         errorCtx.setInvalidParameterError("LogicalComparator constructor", "input 1 pitchBytes must match input 0");
      }
      else if (pDefaultTunableParams->opType > 5U) {
         errorCtx.setInvalidParameterError("LogicalComparator constructor", "opType is out of range");
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

      DSPLIB_logicalComparator_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_logicalComparator_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("LogicalComparator constructor", "failed to allocate logicalComparator handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn0 = TISP_toDsplibBufParams2D<Tin>(_in[0]);
         DSPLIB_bufParams2D_t bufParamsIn1 = TISP_toDsplibBufParams2D<Tin>(_in[1]);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<Tout>(_out);
         DSPLIB_STATUS status = DSPLIB_logicalComparator_init_checkParams(_handle.get(), &bufParamsIn0, &bufParamsIn1,
                                                                          &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_logicalComparator_init_checkParams")) {
            status =
                DSPLIB_logicalComparator_init(_handle.get(), &bufParamsIn0, &bufParamsIn1, &bufParamsOut, &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "DSPLIB_logicalComparator_init")) {
               DSPLIB_logicalComparator_SetArgs kerSetArgs;
               kerSetArgs.opType = _opType;
               status            = DSPLIB_logicalComparator_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(status, "DSPLIB_logicalComparator_set");
            }
         }
      }
   }
}

template <typename Tin, typename Tout> void *LogicalComparator<Tin, Tout>::getInputAddr(size_t index) const noexcept
{
   return (index < 2U) ? _inputs[index] : nullptr;
}

template <typename Tin, typename Tout> size_t LogicalComparator<Tin, Tout>::getNumInputs() const noexcept { return 2U; }

template <typename Tin, typename Tout> void *LogicalComparator<Tin, Tout>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename Tin, typename Tout> size_t LogicalComparator<Tin, Tout>::getNumOutputs() const noexcept { return 1; }

template <typename Tin, typename Tout>
void LogicalComparator<Tin, Tout>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
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

template <typename Tin, typename Tout> void LogicalComparator<Tin, Tout>::exec() noexcept
{
   DSPLIB_logicalComparator_exec(_handle.get(), _in[0].pBuf, _in[1].pBuf, _out.pBuf);
}

template <typename Tin, typename Tout>
bool LogicalComparator<Tin, Tout>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("LogicalComparator::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<LogicalComparatorParamId>(paramId)) {
      case LogicalComparatorParamId::kOpType: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("LogicalComparator::setParams",
                                              "Value size smaller than expected for kOpType");
         }
         else {
            uint32_t opType;
            memcpy(&opType, value, sizeof(uint32_t));
            if (opType > 5U) {
               errorCtx.setInvalidParameterError("LogicalComparator::setParams", "opType is out of range");
            }
            else {
               _opType = opType;
            }
         }
         break;
      }
      case LogicalComparatorParamId::kAll: {
         const LogicalComparatorTunableParams<Tin> *pParams =
             static_cast<const LogicalComparatorTunableParams<Tin> *>(value);
         if (size < sizeof(LogicalComparatorTunableParams<Tin>)) {
            errorCtx.setInvalidParameterError("LogicalComparator::setParams",
                                              "Value size smaller than expected for kAll");
         }
         else if (pParams->opType > 5U) {
            errorCtx.setInvalidParameterError("LogicalComparator::setParams", "opType is out of range");
         }
         else {
            _opType = pParams->opType;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("LogicalComparator::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_logicalComparator_SetArgs kerSetArgs;
         kerSetArgs.opType          = _opType;
         const DSPLIB_STATUS status = DSPLIB_logicalComparator_set(_handle.get(), &kerSetArgs);
         errorCtx.remapXLIBError(status, "DSPLIB_logicalComparator_set");
      }
   }

   return errorCtx.isSuccess();
}

template <typename Tin, typename Tout>
bool LogicalComparator<Tin, Tout>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("LogicalComparator::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("LogicalComparator::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<LogicalComparatorParamId>(paramId)) {
      case LogicalComparatorParamId::kOpType: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("LogicalComparator::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_opType, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case LogicalComparatorParamId::kAll: {
         LogicalComparatorTunableParams<Tin> *pParams = static_cast<LogicalComparatorTunableParams<Tin> *>(value);
         if (*size < sizeof(LogicalComparatorTunableParams<Tin>)) {
            errorCtx.setInvalidParameterError("LogicalComparator::getParams", "Value size smaller than expected");
         }
         else {
            pParams->opType = _opType;
         }
         *size = sizeof(LogicalComparatorTunableParams<Tin>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("LogicalComparator::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class LogicalComparator<int8_t, uint8_t>;
template class LogicalComparator<uint8_t, uint8_t>;
template class LogicalComparator<int16_t, uint16_t>;
template class LogicalComparator<uint16_t, uint16_t>;
template class LogicalComparator<int32_t, uint32_t>;
template class LogicalComparator<uint32_t, uint32_t>;
template class LogicalComparator<int64_t, uint64_t>;
template class LogicalComparator<uint64_t, uint64_t>;
template class LogicalComparator<float, uint32_t>;
template class LogicalComparator<double, uint64_t>;

} // namespace DSPLIB
} // namespace TISP
