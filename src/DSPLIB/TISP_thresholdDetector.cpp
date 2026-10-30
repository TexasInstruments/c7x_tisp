// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_thresholdDetector.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename Tin, typename Tout>
ThresholdDetector<Tin, Tout>::ThresholdDetector(const TISP_BufDescList                    *pInputs,
                                                const TISP_BufDescList                    *pOutputs,
                                                const ThresholdDetectorTunableParams<Tin> *pDefaultTunableParams,
                                                const char                                *nodeName,
                                                uint32_t                                   nodeId,
                                                ErrorCtxt                                 &errorCtx)
    : _in{}, _out{}, _threshold(0), _opType(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("ThresholdDetector constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else {
      /* Nothing to do here */
   }

   if (TISP_checkBufDescList<Tin>(pInputs, 1U, 1U, "ThresholdDetector constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<Tout>(pOutputs, 1U, 1U, "ThresholdDetector constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("ThresholdDetector constructor", "input and output interleave must match");
      }
      // DSPLIB_thresholdDetector_init_checkParams does not compare the output dimensions with the input
      else if ((_out.width != _in.width) || (_out.height != _in.height)) {
         errorCtx.setInvalidParameterError("ThresholdDetector constructor",
                                           "output width and height must match the input");
      }
      else if (pDefaultTunableParams->opType > 5U) {
         errorCtx.setInvalidParameterError("ThresholdDetector constructor", "opType is out of range");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _threshold = pDefaultTunableParams->threshold;
      _opType    = pDefaultTunableParams->opType;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,isInterleave=%u",
                      _in.width, _in.height, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      DSPLIB_thresholdDetector_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_thresholdDetector_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("ThresholdDetector constructor", "failed to allocate thresholdDetector handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn  = TISP_toDsplibBufParams2D<Tin>(_in);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<Tout>(_out);
         DSPLIB_bufParams1D_t bufParamsThreshold;
         bufParamsThreshold.data_type = static_cast<uint32_t>(DSPLIB_element_dType<Tin>::value);
         bufParamsThreshold.dim_x     = 1U;

         DSPLIB_STATUS status = DSPLIB_thresholdDetector_init_checkParams(
             _handle.get(), &bufParamsIn, &bufParamsThreshold, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_thresholdDetector_init_checkParams")) {
            status = DSPLIB_thresholdDetector_init(_handle.get(), &bufParamsIn, &bufParamsThreshold, &bufParamsOut,
                                                   &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "DSPLIB_thresholdDetector_init")) {
               DSPLIB_thresholdDetector_SetArgs kerSetArgs;
               kerSetArgs.opType = _opType;
               status            = DSPLIB_thresholdDetector_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(status, "DSPLIB_thresholdDetector_set");
            }
         }
      }
   }
}

template <typename Tin, typename Tout> void *ThresholdDetector<Tin, Tout>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename Tin, typename Tout> size_t ThresholdDetector<Tin, Tout>::getNumInputs() const noexcept { return 1; }

template <typename Tin, typename Tout> void *ThresholdDetector<Tin, Tout>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename Tin, typename Tout> size_t ThresholdDetector<Tin, Tout>::getNumOutputs() const noexcept { return 1; }

template <typename Tin, typename Tout>
void ThresholdDetector<Tin, Tout>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename Tin, typename Tout> void ThresholdDetector<Tin, Tout>::exec() noexcept
{
   DSPLIB_thresholdDetector_exec(_handle.get(), _in.pBuf, &_threshold, _out.pBuf);
}

template <typename Tin, typename Tout>
bool ThresholdDetector<Tin, Tout>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("ThresholdDetector::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<ThresholdDetectorParamId>(paramId)) {
      case ThresholdDetectorParamId::kThreshold: {
         if (size < sizeof(Tin)) {
            errorCtx.setInvalidParameterError("ThresholdDetector::setParams",
                                              "Value size smaller than expected for kThreshold");
         }
         else {
            memcpy(&_threshold, value, sizeof(Tin));
         }
         break;
      }
      case ThresholdDetectorParamId::kOpType: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("ThresholdDetector::setParams",
                                              "Value size smaller than expected for kOpType");
         }
         else {
            uint32_t opType;
            memcpy(&opType, value, sizeof(uint32_t));
            if (opType > 5U) {
               errorCtx.setInvalidParameterError("ThresholdDetector::setParams", "opType is out of range");
            }
            else {
               _opType = opType;
            }
         }
         break;
      }
      case ThresholdDetectorParamId::kAll: {
         const ThresholdDetectorTunableParams<Tin> *pParams =
             static_cast<const ThresholdDetectorTunableParams<Tin> *>(value);
         if (size < sizeof(ThresholdDetectorTunableParams<Tin>)) {
            errorCtx.setInvalidParameterError("ThresholdDetector::setParams",
                                              "Value size smaller than expected for kAll");
         }
         else if (pParams->opType > 5U) {
            errorCtx.setInvalidParameterError("ThresholdDetector::setParams", "opType is out of range");
         }
         else {
            _threshold = pParams->threshold;
            _opType    = pParams->opType;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("ThresholdDetector::setParams", "Invalid paramId");
         break;
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_thresholdDetector_SetArgs kerSetArgs;
         kerSetArgs.opType          = _opType;
         const DSPLIB_STATUS status = DSPLIB_thresholdDetector_set(_handle.get(), &kerSetArgs);
         errorCtx.remapXLIBError(status, "DSPLIB_thresholdDetector_set");
      }
   }

   return errorCtx.isSuccess();
}

template <typename Tin, typename Tout>
bool ThresholdDetector<Tin, Tout>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("ThresholdDetector::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("ThresholdDetector::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<ThresholdDetectorParamId>(paramId)) {
      case ThresholdDetectorParamId::kThreshold: {
         if (*size < sizeof(Tin)) {
            errorCtx.setInvalidParameterError("ThresholdDetector::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_threshold, sizeof(Tin));
         }
         *size = sizeof(Tin);
         break;
      }
      case ThresholdDetectorParamId::kOpType: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("ThresholdDetector::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_opType, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case ThresholdDetectorParamId::kAll: {
         ThresholdDetectorTunableParams<Tin> *pParams = static_cast<ThresholdDetectorTunableParams<Tin> *>(value);
         if (*size < sizeof(ThresholdDetectorTunableParams<Tin>)) {
            errorCtx.setInvalidParameterError("ThresholdDetector::getParams", "Value size smaller than expected");
         }
         else {
            pParams->threshold = _threshold;
            pParams->opType    = _opType;
         }
         *size = sizeof(ThresholdDetectorTunableParams<Tin>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("ThresholdDetector::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class ThresholdDetector<int8_t, uint8_t>;
template class ThresholdDetector<uint8_t, uint8_t>;
template class ThresholdDetector<int16_t, uint16_t>;
template class ThresholdDetector<uint16_t, uint16_t>;
template class ThresholdDetector<int32_t, uint32_t>;
template class ThresholdDetector<uint32_t, uint32_t>;
template class ThresholdDetector<int64_t, uint64_t>;
template class ThresholdDetector<uint64_t, uint64_t>;
template class ThresholdDetector<float, uint32_t>;
template class ThresholdDetector<double, uint64_t>;

} // namespace DSPLIB
} // namespace TISP
