// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include <TISP_cascadeBiquad.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [DSPLIB_cascadeBiquad constructor]

template <typename T>
CascadeBiquad<T>::CascadeBiquad(const TISP_BufDescList              *pInputs,
                                const TISP_BufDescList              *pOutputs,
                                const TISP_BufDescList              *pStates,
                                const CascadeBiquadConfig      *pConfig,
                                const CascadeBiquadTunableParams<T> *pDefaultTunableParams,
                                const char                          *nodeName,
                                uint32_t                             nodeId,
                                ErrorCtxt                           &errorCtx)
    : _in{}, _out{}, _state{}, _coeffs(nullptr), _coeffSizeBytes(0U), _bypassMode(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pCoeffs == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad constructor",
                                   "coefficient pointer pDefaultTunableParams->pCoeffs is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output, and one state buffer
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "CascadeBiquad constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "CascadeBiquad constructor (outputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pStates, 1U, 1U, "CascadeBiquad constructor (states)", errorCtx)) {
      _in    = pInputs->pDesc[0];
      _out   = pOutputs->pDesc[0];
      _state = pStates->pDesc[0];

      if ((_in.isInterleave != 1U) || (_out.isInterleave != 1U)) {
         errorCtx.setInvalidParameterError("CascadeBiquad constructor", "input and output must be interleaved");
      }
      else if (pDefaultTunableParams->bypassMode > 1U) {
         errorCtx.setInvalidParameterError("CascadeBiquad constructor", "bypass mode must be either zero or one");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _coeffs     = pDefaultTunableParams->pCoeffs;
      _bypassMode = pDefaultTunableParams->bypassMode;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "size=%zu,numChannels=%zu,biquadForm=%u",
                      _in.height, _in.width, pConfig->biquadForm);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***************************************************************************/
      /* Initialize data structures required by DSPLIB_cascadeBiquad from DSPLIB */
      /***************************************************************************/

      DSPLIB_cascadeBiquad_InitArgs kerInitArgs{};
      kerInitArgs.funcStyle       = DSPLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.dataSize        = static_cast<uint32_t>(_in.height);
      kerInitArgs.numChannels     = static_cast<uint32_t>(_in.width);
      kerInitArgs.numStages       = static_cast<uint32_t>(pConfig->numStages);
      kerInitArgs.enableNChCoeffs = pConfig->enableNChCoeffs ? 1U : 0U;
      kerInitArgs.biquadForm      = pConfig->biquadForm;

      DSPLIB_bufParams2D_t bufParamsIn        = TISP_toDsplibBufParams2D<T>(_in);
      DSPLIB_bufParams2D_t bufParamsOut       = TISP_toDsplibBufParams2D<T>(_out);
      DSPLIB_bufParams2D_t bufParamsFilterVar = TISP_toDsplibBufParams2D<T>(_state);
      DSPLIB_bufParams2D_t bufParamsFilterCoeff;

      DSPLIB_STATUS status = DSPLIB_cascadeBiquad_get_coefficientsBufferDesc(
          static_cast<uint32_t>(DSPLIB_element_dType<T>::value), kerInitArgs.numStages, kerInitArgs.numChannels,
          kerInitArgs.biquadForm, kerInitArgs.enableNChCoeffs, &bufParamsFilterCoeff);

      if (errorCtx.remapXLIBError(status, "DSPLIB_cascadeBiquad_get_coefficientsBufferDesc")) {
         _coeffSizeBytes = static_cast<size_t>(bufParamsFilterCoeff.dim_y) *
                           static_cast<size_t>(bufParamsFilterCoeff.stride_y);

         _handle = make_unique_handle<char>(DSPLIB_cascadeBiquad_getHandleSize(&kerInitArgs));

         if (!_handle) {
            errorCtx.setMemoryError("CascadeBiquad constructor", "failed to allocate cascadeBiquad handle");
         }
         else {
            /******************************************************************************/
            /* Validate parameters before init                                            */
            /******************************************************************************/

            status = DSPLIB_cascadeBiquad_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsFilterCoeff,
                                                           &bufParamsFilterVar, &bufParamsOut, &kerInitArgs);

            /******************************************************************************/
            /* Call init function from DSPLIB cascade biquad to set SE/SA params          */
            /******************************************************************************/

            if (errorCtx.remapXLIBError(status, "DSPLIB_cascadeBiquad_init_checkParams")) {
               status = DSPLIB_cascadeBiquad_init(_handle.get(), &bufParamsIn, &bufParamsFilterCoeff,
                                                  &bufParamsFilterVar, &bufParamsOut, &kerInitArgs);
               if (errorCtx.remapXLIBError(status, "DSPLIB_cascadeBiquad_init")) {
                  cleanInternalBuffers();
               }
            }
         }
      }
   }
}

//! [DSPLIB_CascadeBiquad constructor]

template <typename T> void *CascadeBiquad<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *CascadeBiquad<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t CascadeBiquad<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t CascadeBiquad<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void CascadeBiquad<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addr.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void CascadeBiquad<T>::bypassCopy() noexcept
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

template <typename T> void CascadeBiquad<T>::exec() noexcept
{
   if (_bypassMode != 0U) {
      bypassCopy();
   }
   else {
      DSPLIB_cascadeBiquad_exec(_handle.get(), _in.pBuf, _coeffs, _state.pBuf, _out.pBuf);
   }
}

template <typename T>
bool CascadeBiquad<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<CascadeBiquadParamId>(paramId)) {
      case CascadeBiquadParamId::kFilterCoeffs: {
         if (size < _coeffSizeBytes) {
            errorCtx.setInvalidParameterError("CascadeBiquad::setParams",
                                              "Value size smaller than expected for kFilterCoeffs");
         }
         else {
            cleanInternalBuffers();
            if (value != _coeffs) {
               (void) memcpy(_coeffs, value, _coeffSizeBytes);
            }
         }
         break;
      }
      case CascadeBiquadParamId::kBypass: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("CascadeBiquad::setParams",
                                              "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode = 0U;
            (void) memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("CascadeBiquad::setParams", "bypass mode must be either zero or one");
            }
            else {
               // Leaving bypass: the filter variables are stale, so resume from a cold start
               if ((_bypassMode != 0U) && (bypassMode == 0U)) {
                  cleanInternalBuffers();
               }
               _bypassMode = bypassMode;
            }
         }
         break;
      }
      case CascadeBiquadParamId::kAll: {
         const CascadeBiquadTunableParams<T> *pParams = static_cast<const CascadeBiquadTunableParams<T> *>(value);
         if (size < sizeof(CascadeBiquadTunableParams<T>)) {
            errorCtx.setInvalidParameterError("CascadeBiquad::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pCoeffs == nullptr) {
            errorCtx.setNullPointerError("CascadeBiquad::setParams", "coefficient pointer pCoeffs is null");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("CascadeBiquad::setParams", "bypass mode must be either zero or one");
         }
         else {
            if (pParams->pCoeffs != _coeffs) {
               (void) memcpy(_coeffs, pParams->pCoeffs, _coeffSizeBytes);
            }
            cleanInternalBuffers();
            _bypassMode = pParams->bypassMode;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("CascadeBiquad::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool CascadeBiquad<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (size == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<CascadeBiquadParamId>(paramId)) {
      case CascadeBiquadParamId::kFilterCoeffs: {
         if (value == nullptr) {
            errorCtx.setNullPointerError("CascadeBiquad::getParams", "value pointer is null");
         }
         else if (*size < _coeffSizeBytes) {
            errorCtx.setInvalidParameterError("CascadeBiquad::getParams", "Provided buffer size is too small");
         }
         else if (value != _coeffs) {
            (void) memcpy(value, _coeffs, _coeffSizeBytes);
         }
         else {
            /* Nothing to do here */
         }
         *size = _coeffSizeBytes;
         break;
      }
      case CascadeBiquadParamId::kBypass: {
         if (value == nullptr) {
            errorCtx.setNullPointerError("CascadeBiquad::getParams", "value pointer is null");
         }
         else if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("CascadeBiquad::getParams",
                                              "Provided buffer size is too small for kBypass");
         }
         else {
            (void) memcpy(value, &_bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case CascadeBiquadParamId::kAll: {
         CascadeBiquadTunableParams<T> *pParams = static_cast<CascadeBiquadTunableParams<T> *>(value);
         if (pParams == nullptr) {
            errorCtx.setNullPointerError("CascadeBiquad::getParams", "value pointer is null");
         }
         else if (*size < sizeof(CascadeBiquadTunableParams<T>)) {
            errorCtx.setInvalidParameterError("CascadeBiquad::getParams", "Provided buffer size is too small for kAll");
         }
         else if (pParams->pCoeffs == nullptr) {
            errorCtx.setNullPointerError("CascadeBiquad::getParams", "coefficient pointer pCoeffs is null");
         }
         else {
            if (pParams->pCoeffs != _coeffs) {
               (void) memcpy(pParams->pCoeffs, _coeffs, _coeffSizeBytes);
            }
            pParams->bypassMode = _bypassMode;
         }
         *size = sizeof(CascadeBiquadTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("CascadeBiquad::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> void CascadeBiquad<T>::cleanInternalBuffers() noexcept
{
   if (_state.pBuf != nullptr) {
      (void) memset(_state.pBuf, 0, _state.pitchBytes * _state.height);
   }
}

template class CascadeBiquad<float>;
template class CascadeBiquad<double>;

} // namespace DSPLIB
} // namespace TISP
