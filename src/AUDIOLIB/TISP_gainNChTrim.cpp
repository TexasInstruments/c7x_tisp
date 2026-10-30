// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_gainNChTrim.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::GainNChTrim constructor]

template <typename T>
GainNChTrim<T>::GainNChTrim(const TISP_BufDescList            *pInputs,
                            const TISP_BufDescList            *pOutputs,
                            const GainNChTrimTunableParams<T> *pDefaultTunableParams,
                            const char                        *nodeName,
                            uint32_t                           nodeId,
                            ErrorCtxt                         &errorCtx)
    : _in{}, _out{}, _numChannels(0U), _bypassMode(0U), _gains(nullptr), _masterGain(nullptr)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pGains == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim constructor", "gain pointer pDefaultTunableParams->pGains is null");
   }
   else if (pDefaultTunableParams->pMasterGain == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim constructor",
                                   "master gain pointer pDefaultTunableParams->pMasterGain is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "GainNChTrim constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "GainNChTrim constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("GainNChTrim constructor", "input and output interleave must match");
      }
      else if (pDefaultTunableParams->bypassMode > 1U) {
         errorCtx.setInvalidParameterError("GainNChTrim constructor", "bypass mode must be either zero or one");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // One gain per channel: channels are columns when interleaved, rows otherwise
      _numChannels = (_in.isInterleave == 1U) ? _in.width : _in.height;
      _bypassMode  = pDefaultTunableParams->bypassMode;
      _gains       = pDefaultTunableParams->pGains;
      _masterGain  = pDefaultTunableParams->pMasterGain;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "width=%zu, height=%zu, numChannels=%zu, isInterleave=%u, masterGain=%.2f", _in.width, _in.height,
                      _numChannels, static_cast<uint32_t>(_in.isInterleave), static_cast<double>(*_masterGain));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***************************************************************************/
      /* Initialize data structures required by AUDIOLIB_gainNChTrim from AUDIOLIB */
      /***************************************************************************/

      AUDIOLIB_gainNChTrim_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = _in.isInterleave;

      _handle = make_unique_handle<char>(AUDIOLIB_gainNChTrim_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("GainNChTrim constructor", "failed to allocate gainNChTrim handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);
         AUDIOLIB_bufParams1D_t bufParamsGain;

         bufParamsGain.data_type = static_cast<uint32_t>(AUDIOLIB_element_dType<T>::value);
         bufParamsGain.dim_x     = static_cast<uint32_t>(_numChannels);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status = AUDIOLIB_gainNChTrim_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsGain,
                                                                        &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for gainNChTrim to set SE/SA params       */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "AUDIOLIB_gainNChTrim_init_checkParams")) {
            status =
                AUDIOLIB_gainNChTrim_init(_handle.get(), &bufParamsIn, &bufParamsGain, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_gainNChTrim_init");
         }
      }
   }
}

template <typename T> void *GainNChTrim<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *GainNChTrim<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t GainNChTrim<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t GainNChTrim<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::GainNChTrim setAddr]
template <typename T> void GainNChTrim<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void GainNChTrim<T>::bypassCopy() noexcept
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

template <typename T> void GainNChTrim<T>::exec() noexcept
{
   if (_bypassMode != 0U) {
      bypassCopy();
   }
   else {
      AUDIOLIB_gainNChTrim_exec(_handle.get(), _in.pBuf, _gains, _masterGain, _out.pBuf);
   }
}

template <typename T>
bool GainNChTrim<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<GainNChTrimParamId>(paramId)) {
      case GainNChTrimParamId::kGainValues: {
         size_t expectedSize = _numChannels * sizeof(T);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNChTrim::setParams",
                                              "Value size smaller than expected for kGainValues");
         }
         else if (value != _gains) {
            memcpy(_gains, value, expectedSize);
         }
         else {
            /* Nothing to do here */
         }
         break;
      }
      case GainNChTrimParamId::kMasterGain: {
         if (size < sizeof(T)) {
            errorCtx.setInvalidParameterError("GainNChTrim::setParams",
                                              "Value size smaller than expected for kMasterGain");
         }
         else if (value != _masterGain) {
            memcpy(_masterGain, value, sizeof(T));
         }
         else {
            /* Nothing to do here */
         }
         break;
      }
      case GainNChTrimParamId::kBypass: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainNChTrim::setParams", "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode = 0U;
            memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("GainNChTrim::setParams", "bypass mode must be either zero or one");
            }
            else {
               _bypassMode = bypassMode;
            }
         }
         break;
      }
      case GainNChTrimParamId::kAll: {
         const GainNChTrimTunableParams<T> *pParams = static_cast<const GainNChTrimTunableParams<T> *>(value);
         if (size < sizeof(GainNChTrimTunableParams<T>)) {
            errorCtx.setInvalidParameterError("GainNChTrim::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pGains == nullptr) {
            errorCtx.setNullPointerError("GainNChTrim::setParams", "gain pointer pGains is null");
         }
         else if (pParams->pMasterGain == nullptr) {
            errorCtx.setNullPointerError("GainNChTrim::setParams", "master gain pointer pMasterGain is null");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("GainNChTrim::setParams", "bypass mode must be either zero or one");
         }
         else {
            if (pParams->pGains != _gains) {
               memcpy(_gains, pParams->pGains, _numChannels * sizeof(T));
            }
            if (pParams->pMasterGain != _masterGain) {
               memcpy(_masterGain, pParams->pMasterGain, sizeof(T));
            }
            _bypassMode = pParams->bypassMode;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("GainNChTrim::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool GainNChTrim<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<GainNChTrimParamId>(paramId)) {
      case GainNChTrimParamId::kGainValues: {
         size_t expectedSize = _numChannels * sizeof(T);
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNChTrim::getParams",
                                              "Value size smaller than expected for kGainValues");
         }
         else if (value != _gains) {
            memcpy(value, _gains, expectedSize);
         }
         else {
            /* Nothing to do here */
         }
         *size = expectedSize;
         break;
      }
      case GainNChTrimParamId::kMasterGain: {
         if (*size < sizeof(T)) {
            errorCtx.setInvalidParameterError("GainNChTrim::getParams",
                                              "Value size smaller than expected for kMasterGain");
         }
         else if (value != _masterGain) {
            memcpy(value, _masterGain, sizeof(T));
         }
         else {
            /* Nothing to do here */
         }
         *size = sizeof(T);
         break;
      }
      case GainNChTrimParamId::kBypass: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainNChTrim::getParams", "Value size smaller than expected for kBypass");
         }
         else {
            memcpy(value, &_bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case GainNChTrimParamId::kAll: {
         GainNChTrimTunableParams<T> *pParams = static_cast<GainNChTrimTunableParams<T> *>(value);
         if (*size < sizeof(GainNChTrimTunableParams<T>)) {
            errorCtx.setInvalidParameterError("GainNChTrim::getParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pGains == nullptr) {
            errorCtx.setNullPointerError("GainNChTrim::getParams", "gain pointer pGains is null");
         }
         else if (pParams->pMasterGain == nullptr) {
            errorCtx.setNullPointerError("GainNChTrim::getParams", "master gain pointer pMasterGain is null");
         }
         else {
            if (pParams->pGains != _gains) {
               memcpy(pParams->pGains, _gains, _numChannels * sizeof(T));
            }
            if (pParams->pMasterGain != _masterGain) {
               memcpy(pParams->pMasterGain, _masterGain, sizeof(T));
            }
            pParams->bypassMode = _bypassMode;
         }
         *size = sizeof(GainNChTrimTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("GainNChTrim::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class GainNChTrim<float>;
template class GainNChTrim<double>;

} // namespace AUDIOLIB
} // namespace TISP
