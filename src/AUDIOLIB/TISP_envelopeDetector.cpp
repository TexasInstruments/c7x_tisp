// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_envelopeDetector.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
EnvelopeDetector<T>::EnvelopeDetector(const TISP_BufDescList                 *pInputs,
                                      const TISP_BufDescList                 *pOutputs,
                                      const EnvelopeDetectorConfig           *pConfig,
                                      const EnvelopeDetectorTunableParams<T> *pDefaultTunableParams,
                                      const char                             *nodeName,
                                      uint32_t                                nodeId,
                                      ErrorCtxt                              &errorCtx)
    : _in{}, _out{}, _config{}, _tunableParams{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("EnvelopeDetector constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("EnvelopeDetector constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else {
      _config        = *pConfig;
      _tunableParams = *pDefaultTunableParams;
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "EnvelopeDetector constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "EnvelopeDetector constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("EnvelopeDetector constructor", "input 0 must be planar");
      }
      else if (_out.isInterleave != 0U) {
         errorCtx.setInvalidParameterError("EnvelopeDetector constructor", "output 0 must be planar");
      }
      else if ((_config.samplingRate < 1) || (_config.samplingRate > 192000)) {
         errorCtx.setInvalidParameterError("EnvelopeDetector constructor", "samplingRate is out of range");
      }
      else if ((_tunableParams.attackTime < 1e-05) || (_tunableParams.attackTime > 10.0f)) {
         errorCtx.setInvalidParameterError("EnvelopeDetector constructor", "attackTime is out of range");
      }
      else if ((_tunableParams.releaseTime < 1e-05) || (_tunableParams.releaseTime > 10.0f)) {
         errorCtx.setInvalidParameterError("EnvelopeDetector constructor", "releaseTime is out of range");
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

      AUDIOLIB_envelopeDetector_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.samplingRate = _config.samplingRate;

      _handle = make_unique_handle<char>(AUDIOLIB_envelopeDetector_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("EnvelopeDetector constructor", "failed to allocate envelopeDetector handle");
      }
      else if (_in.pBuf == _out.pBuf) {
         errorCtx.setInvalidParameterError("EnvelopeDetector constructor", "in-place operation is not supported");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);

         AUDIOLIB_STATUS status =
             AUDIOLIB_envelopeDetector_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_envelopeDetector_init_checkParams")) {
            status = AUDIOLIB_envelopeDetector_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         }
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_envelopeDetector_init")) {
            AUDIOLIB_envelopeDetector_SetArgs kerSetArgs;
            kerSetArgs.attackTime  = _tunableParams.attackTime;
            kerSetArgs.releaseTime = _tunableParams.releaseTime;
            status                 = AUDIOLIB_envelopeDetector_set(_handle.get(), &kerSetArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_envelopeDetector_set");
         }
      }
   }
}

template <typename T> void *EnvelopeDetector<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t EnvelopeDetector<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *EnvelopeDetector<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t EnvelopeDetector<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void EnvelopeDetector<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void EnvelopeDetector<T>::exec() noexcept
{
   AUDIOLIB_envelopeDetector_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template <typename T>
bool EnvelopeDetector<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("EnvelopeDetector::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<EnvelopeDetectorParamId>(paramId)) {
      case EnvelopeDetectorParamId::kAttackTime: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("EnvelopeDetector::setParams",
                                              "Value size smaller than expected for kAttackTime");
         }
         else {
            float attackTime;
            memcpy(&attackTime, value, sizeof(float));
            if ((attackTime < 1e-05) || (attackTime > 10.0f)) {
               errorCtx.setInvalidParameterError("EnvelopeDetector::setParams", "attackTime is out of range");
            }
            else {
               _tunableParams.attackTime = attackTime;
            }
         }
         break;
      }
      case EnvelopeDetectorParamId::kReleaseTime: {
         if (size < sizeof(float)) {
            errorCtx.setInvalidParameterError("EnvelopeDetector::setParams",
                                              "Value size smaller than expected for kReleaseTime");
         }
         else {
            float releaseTime;
            memcpy(&releaseTime, value, sizeof(float));
            if ((releaseTime < 1e-05) || (releaseTime > 10.0f)) {
               errorCtx.setInvalidParameterError("EnvelopeDetector::setParams", "releaseTime is out of range");
            }
            else {
               _tunableParams.releaseTime = releaseTime;
            }
         }
         break;
      }
      case EnvelopeDetectorParamId::kAll: {
         const EnvelopeDetectorTunableParams<T> *pParams = static_cast<const EnvelopeDetectorTunableParams<T> *>(value);
         if (size < sizeof(EnvelopeDetectorTunableParams<T>)) {
            errorCtx.setInvalidParameterError("EnvelopeDetector::setParams",
                                              "Value size smaller than expected for kAll");
         }
         else if ((pParams->attackTime < 1e-05) || (pParams->attackTime > 10.0f)) {
            errorCtx.setInvalidParameterError("EnvelopeDetector::setParams", "attackTime is out of range");
         }
         else if ((pParams->releaseTime < 1e-05) || (pParams->releaseTime > 10.0f)) {
            errorCtx.setInvalidParameterError("EnvelopeDetector::setParams", "releaseTime is out of range");
         }
         else {
            _tunableParams.attackTime  = pParams->attackTime;
            _tunableParams.releaseTime = pParams->releaseTime;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("EnvelopeDetector::setParams", "Invalid paramId");
         break;
      }
   }

   // The kernel holds the parameters: hand it the new values; its state (gain, envelope) is kept
   if (errorCtx.isSuccess()) {
      AUDIOLIB_envelopeDetector_SetArgs kerSetArgs;
      kerSetArgs.attackTime  = _tunableParams.attackTime;
      kerSetArgs.releaseTime = _tunableParams.releaseTime;
      AUDIOLIB_STATUS status = AUDIOLIB_envelopeDetector_set(_handle.get(), &kerSetArgs);
      errorCtx.remapXLIBError(status, "AUDIOLIB_envelopeDetector_set");
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool EnvelopeDetector<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("EnvelopeDetector::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("EnvelopeDetector::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<EnvelopeDetectorParamId>(paramId)) {
      case EnvelopeDetectorParamId::kAttackTime: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("EnvelopeDetector::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.attackTime, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case EnvelopeDetectorParamId::kReleaseTime: {
         if (*size < sizeof(float)) {
            errorCtx.setInvalidParameterError("EnvelopeDetector::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_tunableParams.releaseTime, sizeof(float));
         }
         *size = sizeof(float);
         break;
      }
      case EnvelopeDetectorParamId::kAll: {
         EnvelopeDetectorTunableParams<T> *pParams = static_cast<EnvelopeDetectorTunableParams<T> *>(value);
         if (*size < sizeof(EnvelopeDetectorTunableParams<T>)) {
            errorCtx.setInvalidParameterError("EnvelopeDetector::getParams", "Value size smaller than expected");
         }
         else {
            pParams->attackTime  = _tunableParams.attackTime;
            pParams->releaseTime = _tunableParams.releaseTime;
         }
         *size = sizeof(EnvelopeDetectorTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("EnvelopeDetector::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class EnvelopeDetector<float>;
template class EnvelopeDetector<double>;

} // namespace AUDIOLIB
} // namespace TISP
