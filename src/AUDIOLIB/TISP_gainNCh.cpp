// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_gainNCh.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::GainNCh constructor]

template <typename T>
GainNCh<T>::GainNCh(const TISP_BufDescList        *pInputs,
                    const TISP_BufDescList        *pOutputs,
                    const GainNChTunableParams<T> *pDefaultTunableParams,
                    const char                    *nodeName,
                    uint32_t                       nodeId,
                    ErrorCtxt                     &errorCtx)
    : _in{}, _out{}, _numChannels(0U), _bypassMode(0U), _gains(nullptr)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("GainNCh constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pGains == nullptr) {
      errorCtx.setNullPointerError("GainNCh constructor", "gain pointer pDefaultTunableParams->pGains is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "GainNCh constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "GainNCh constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("GainNCh constructor", "input and output interleave must match");
      }
      else if (pDefaultTunableParams->bypassMode > 1U) {
         errorCtx.setInvalidParameterError("GainNCh constructor", "bypass mode must be either zero or one");
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

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "width=%zu, height=%zu, numChannels=%zu, isInterleave=%u", _in.width, _in.height, _numChannels,
                      static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_gainNCh from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_gainNCh_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = _in.isInterleave;

      _handle = make_unique_handle<char>(AUDIOLIB_gainNCh_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("GainNCh constructor", "failed to allocate gainNCh handle");
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

         AUDIOLIB_STATUS status = AUDIOLIB_gainNCh_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsGain,
                                                                    &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for gainNCh to set SE/SA params          */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "AUDIOLIB_gainNCh_init_checkParams")) {
            status = AUDIOLIB_gainNCh_init(_handle.get(), &bufParamsIn, &bufParamsGain, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_gainNCh_init");
         }
      }
   }
}

template <typename T> void *GainNCh<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *GainNCh<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t GainNCh<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t GainNCh<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::GainNCh setAddr]
template <typename T> void GainNCh<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void GainNCh<T>::bypassCopy() noexcept
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

template <typename T> void GainNCh<T>::exec() noexcept
{
   if (_bypassMode != 0U) {
      bypassCopy();
   }
   else {
      AUDIOLIB_gainNCh_exec(_handle.get(), _in.pBuf, _gains, _out.pBuf);
   }
}

template <typename T>
bool GainNCh<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("GainNCh::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<GainNChParamId>(paramId)) {
      case GainNChParamId::kGainValue: {
         size_t expectedSize = _numChannels * sizeof(T);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNCh::setParams", "Value size smaller than expected for kGainValue");
         }
         else if (value != _gains) {
            memcpy(_gains, value, expectedSize);
         }
         else {
            /* Nothing to do here */
         }
         break;
      }
      case GainNChParamId::kBypass: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainNCh::setParams", "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode = 0U;
            memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("GainNCh::setParams", "bypass mode must be either zero or one");
            }
            else {
               _bypassMode = bypassMode;
            }
         }
         break;
      }
      case GainNChParamId::kAll: {
         const GainNChTunableParams<T> *pParams = static_cast<const GainNChTunableParams<T> *>(value);
         if (size < sizeof(GainNChTunableParams<T>)) {
            errorCtx.setInvalidParameterError("GainNCh::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pGains == nullptr) {
            errorCtx.setNullPointerError("GainNCh::setParams", "gain pointer pGains is null");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("GainNCh::setParams", "bypass mode must be either zero or one");
         }
         else {
            if (pParams->pGains != _gains) {
               memcpy(_gains, pParams->pGains, _numChannels * sizeof(T));
            }
            _bypassMode = pParams->bypassMode;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("GainNCh::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool GainNCh<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (size == nullptr) {
      errorCtx.setNullPointerError("GainNCh::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<GainNChParamId>(paramId)) {
      case GainNChParamId::kGainValue: {
         size_t expectedSize = _numChannels * sizeof(T);
         if (value == nullptr) {
            errorCtx.setNullPointerError("GainNCh::getParams", "value pointer is null");
         }
         else if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNCh::getParams", "Value size smaller than expected");
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
      case GainNChParamId::kBypass: {
         if (value == nullptr) {
            errorCtx.setNullPointerError("GainNCh::getParams", "value pointer is null");
         }
         else if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("GainNCh::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case GainNChParamId::kAll: {
         GainNChTunableParams<T> *pParams = static_cast<GainNChTunableParams<T> *>(value);
         if (pParams == nullptr) {
            errorCtx.setNullPointerError("GainNCh::getParams", "value pointer is null");
         }
         else if (*size < sizeof(GainNChTunableParams<T>)) {
            errorCtx.setInvalidParameterError("GainNCh::getParams", "Value size smaller than expected");
         }
         else if (pParams->pGains == nullptr) {
            errorCtx.setNullPointerError("GainNCh::getParams", "gain pointer pGains is null");
         }
         else {
            if (pParams->pGains != _gains) {
               memcpy(pParams->pGains, _gains, _numChannels * sizeof(T));
            }
            pParams->bypassMode = _bypassMode;
         }
         *size = sizeof(GainNChTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("GainNCh::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class GainNCh<float>;
template class GainNCh<double>;

} // namespace AUDIOLIB
} // namespace TISP
