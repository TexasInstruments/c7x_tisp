// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_rmsSmoothed.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
RmsSmoothed<T>::RmsSmoothed(const TISP_BufDescList            *pInputs,
                            const TISP_BufDescList            *pOutputs,
                            const RmsSmoothedConfig           *pConfig,
                            const RmsSmoothedTunableParams<T> *pDefaultTunableParams,
                            const char                        *nodeName,
                            uint32_t                           nodeId,
                            ErrorCtxt                         &errorCtx)
    : _in{}, _out{}, _config{}, _tunableParams{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("RmsSmoothed constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("RmsSmoothed constructor", "default params pointer pDefaultTunableParams is null");
   }
   else {
      _config        = *pConfig;
      _tunableParams = *pDefaultTunableParams;
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "RmsSmoothed constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "RmsSmoothed constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("RmsSmoothed constructor", "input 0 must be planar");
      }
      else if (_out.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("RmsSmoothed constructor", "output 0 must be planar");
      }
      else if ((_config.samplingRate < 1) || (_config.samplingRate > 192000)) {
         errorCtx.setInvalidParameterError("RmsSmoothed constructor", "samplingRate is out of range");
      }
      else if ((_tunableParams.smoothingTime < 0.0f) || (_tunableParams.smoothingTime > 10000.0f)) {
         errorCtx.setInvalidParameterError("RmsSmoothed constructor", "smoothingTime is out of range");
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

      AUDIOLIB_rmsSmoothed_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate = _config.samplingRate;

      _handle = make_unique_handle<char>(AUDIOLIB_rmsSmoothed_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("RmsSmoothed constructor", "failed to allocate rmsSmoothed handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams1D_t bufParamsOut;
         bufParamsOut.data_type = static_cast<uint32_t>(AUDIOLIB_element_dType<T>::value);
         bufParamsOut.dim_x     = static_cast<uint32_t>(_out.width);

         AUDIOLIB_STATUS status =
             AUDIOLIB_rmsSmoothed_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_rmsSmoothed_init_checkParams")) {
            status = AUDIOLIB_rmsSmoothed_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         }
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_rmsSmoothed_init")) {
            AUDIOLIB_rmsSmoothed_SetArgs kerSetArgs;
            kerSetArgs.smoothingTime = _tunableParams.smoothingTime;
            status                   = AUDIOLIB_rmsSmoothed_set(_handle.get(), &kerSetArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_rmsSmoothed_set");
         }
      }
   }
}

template <typename T> void *RmsSmoothed<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t RmsSmoothed<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *RmsSmoothed<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t RmsSmoothed<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void RmsSmoothed<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void RmsSmoothed<T>::exec() noexcept
{
   AUDIOLIB_rmsSmoothed_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template <typename T>
bool RmsSmoothed<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("RmsSmoothed::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<RmsSmoothedParamId>(paramId)) {
      case RmsSmoothedParamId::kSmoothingTime: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("RmsSmoothed::setParams",
                                              "Value size smaller than expected for kSmoothingTime");
         }
         else {
            float smoothingTime;
            memcpy(&smoothingTime, value, sizeof(float));
            if ((smoothingTime < 0.0f) || (smoothingTime > 10000.0f)) {
               errorCtx.setInvalidParameterError("RmsSmoothed::setParams", "smoothingTime is out of range");
            }
            else {
               _tunableParams.smoothingTime = smoothingTime;
            }
         }
         break;
      }
      case RmsSmoothedParamId::kAll: {
         const RmsSmoothedTunableParams<T> *pParams = static_cast<const RmsSmoothedTunableParams<T> *>(value);
         if (size < sizeof(RmsSmoothedTunableParams<T>)) {
            errorCtx.setInvalidParameterError("RmsSmoothed::setParams", "Value size smaller than expected for kAll");
         }
         else if ((pParams->smoothingTime < 0.0f) || (pParams->smoothingTime > 10000.0f)) {
            errorCtx.setInvalidParameterError("RmsSmoothed::setParams", "smoothingTime is out of range");
         }
         else {
            _tunableParams.smoothingTime = pParams->smoothingTime;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("RmsSmoothed::setParams", "Invalid paramId");
         break;
      }
   }

   // The kernel holds the smoothing time: hand it the new value; the smoothed energy is kept
   if (errorCtx.isSuccess()) {
      AUDIOLIB_rmsSmoothed_SetArgs kerSetArgs;
      kerSetArgs.smoothingTime = _tunableParams.smoothingTime;
      AUDIOLIB_STATUS status   = AUDIOLIB_rmsSmoothed_set(_handle.get(), &kerSetArgs);
      errorCtx.remapXLIBError(status, "AUDIOLIB_rmsSmoothed_set");
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool RmsSmoothed<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("RmsSmoothed::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("RmsSmoothed::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<RmsSmoothedParamId>(paramId)) {
      case RmsSmoothedParamId::kSmoothingTime: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("RmsSmoothed::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.smoothingTime, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case RmsSmoothedParamId::kAll: {
         RmsSmoothedTunableParams<T> *pParams = static_cast<RmsSmoothedTunableParams<T> *>(value);
         if (*size < sizeof(RmsSmoothedTunableParams<T>)) {
            errorCtx.setInvalidParameterError("RmsSmoothed::getParams", "Value size smaller than expected");
         }
         else {
            pParams->smoothingTime = _tunableParams.smoothingTime;
         }
         *size = sizeof(RmsSmoothedTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("RmsSmoothed::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class RmsSmoothed<float>;
template class RmsSmoothed<double>;

} // namespace AUDIOLIB
} // namespace TISP
