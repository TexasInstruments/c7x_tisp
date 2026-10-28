// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_downwardExpander.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::DownwardExpander constructor]

template <typename T>
DownwardExpander<T>::DownwardExpander(const TISP_BufDescList                 *pInputs,
                                      const TISP_BufDescList                 *pOutputs,
                                      const TISP_BufDescList                 *pStates,
                                      void                                   *pScratch,
                                      size_t                                  scratchBytes,
                                      const DownwardExpanderTunableParams<T> *pDefaultTunableParams,
                                      const char                             *nodeName,
                                      uint32_t                                nodeId,
                                      ErrorCtxt                              &errorCtx)
    : _in{}, _out{}, _scratch(static_cast<T *>(pScratch)), _gainState(nullptr), _holdCounter(nullptr),
      _thresholdDb(nullptr), _ratio(nullptr), _kneeWidthDb(nullptr), _attackCoeff(nullptr), _releaseCoeff(nullptr),
      _holdSamples(nullptr), _numChannels(0U), _blockSize(0U), _bypassMode(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("DownwardExpander constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else if (pScratch == nullptr) {
      errorCtx.setNullPointerError("DownwardExpander constructor", "scratch pointer pScratch is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output, and two state buffers
   // (state 0: smoothed gain, state 1: hold counter)
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "DownwardExpander constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "DownwardExpander constructor (outputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pStates, 2U, 2U, "DownwardExpander constructor (states)", errorCtx)) {
      _in          = pInputs->pDesc[0];
      _out         = pOutputs->pDesc[0];
      _numChannels = _in.width;
      _blockSize   = _in.height;

      if ((_in.isInterleave != 1U) || (_out.isInterleave != 1U)) {
         errorCtx.setInvalidParameterError("DownwardExpander constructor", "input and output must be interleaved");
      }
      else if ((_out.width != _in.width) || (_out.height != _in.height)) {
         errorCtx.setInvalidParameterError("DownwardExpander constructor",
                                           "output width and height must match the input");
      }
      else if ((pStates->pDesc[0].width != _numChannels) || (pStates->pDesc[1].width != _numChannels)) {
         errorCtx.setInvalidParameterError("DownwardExpander constructor",
                                           "gain state and hold counter widths must equal the channel count");
      }
      else if (scratchBytes < (_numChannels * _blockSize * sizeof(T))) {
         errorCtx.setInvalidParameterError("DownwardExpander constructor",
                                           "scratchBytes is smaller than channels x samples");
      }
      else if ((pDefaultTunableParams->pThresholdDb == nullptr) || (pDefaultTunableParams->pRatio == nullptr) ||
               (pDefaultTunableParams->pKneeWidthDb == nullptr) || (pDefaultTunableParams->pAttackCoeff == nullptr) ||
               (pDefaultTunableParams->pReleaseCoeff == nullptr) || (pDefaultTunableParams->pHoldSamples == nullptr)) {
         errorCtx.setNullPointerError(
             "DownwardExpander constructor",
             "threshold, ratio, knee width, attack, release, and hold arrays must be non-null");
      }
      else if (pDefaultTunableParams->bypassMode > 1U) {
         errorCtx.setInvalidParameterError("DownwardExpander constructor", "bypass mode must be either zero or one");
      }
      else {
         // Per-channel range checks; the first out-of-range value stops the scan
         for (size_t i = 0; (i < _numChannels) && errorCtx.isSuccess(); i++) {
            if (pDefaultTunableParams->pThresholdDb[i] > static_cast<T>(0.0)) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander constructor", "kThresholdDb must be <= 0.0");
            }
            else if (pDefaultTunableParams->pRatio[i] < static_cast<T>(1.0)) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander constructor", "kRatio must be >= 1.0");
            }
            else if (pDefaultTunableParams->pKneeWidthDb[i] < static_cast<T>(0.0)) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander constructor", "kKneeWidthDb must be >= 0.0");
            }
            else if ((pDefaultTunableParams->pAttackCoeff[i] < static_cast<T>(0.0)) || // NOLINT(MISRA.PTR.ARITH)
                     (pDefaultTunableParams->pAttackCoeff[i] > static_cast<T>(1.0))) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander constructor",
                                                 "kAttackCoeff coefficients must be between 0.0 and 1.0");
            }
            else if ((pDefaultTunableParams->pReleaseCoeff[i] < static_cast<T>(0.0)) || // NOLINT(MISRA.PTR.ARITH)
                     (pDefaultTunableParams->pReleaseCoeff[i] > static_cast<T>(1.0))) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander constructor",
                                                 "kReleaseCoeff coefficients must be between 0.0 and 1.0");
            }
            // The hold counter is a float: it saturates at 2^24 (2^24 + 1 == 2^24 in float32), so a hold of 2^24 or
            // more would never end
            else if ((pDefaultTunableParams->pHoldSamples[i] < static_cast<T>(0.0)) ||        // NOLINT(MISRA.PTR.ARITH)
                     (pDefaultTunableParams->pHoldSamples[i] > static_cast<T>(16777215.0))) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander constructor",
                                                 "kHoldSamples must be between 0.0 and 16777215.0");
            }
            else {
               /* Nothing to do here */
            }
         }
      }
   }

   if (errorCtx.isSuccess()) {
      _gainState    = static_cast<T *>(pStates->pDesc[0].pBuf);
      _holdCounter  = static_cast<T *>(pStates->pDesc[1].pBuf);
      _thresholdDb  = pDefaultTunableParams->pThresholdDb;
      _ratio        = pDefaultTunableParams->pRatio;
      _kneeWidthDb  = pDefaultTunableParams->pKneeWidthDb;
      _attackCoeff  = pDefaultTunableParams->pAttackCoeff;
      _releaseCoeff = pDefaultTunableParams->pReleaseCoeff;
      _holdSamples  = pDefaultTunableParams->pHoldSamples;
      _bypassMode   = pDefaultTunableParams->bypassMode;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numChannels=%zu, blockSize=%zu", _numChannels,
                      _blockSize);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
      AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);
      AUDIOLIB_bufParams2D_t bufParamsScratch;
      AUDIOLIB_bufParams1D_t bufParamsGain;

      bufParamsScratch.data_type = static_cast<uint32_t>(AUDIOLIB_element_dType<T>::value);
      bufParamsScratch.dim_x     = static_cast<uint32_t>(_numChannels);
      bufParamsScratch.dim_y     = static_cast<uint32_t>(_blockSize);
      bufParamsScratch.stride_y  = static_cast<int32_t>(_numChannels * sizeof(T));

      bufParamsGain.data_type = static_cast<uint32_t>(AUDIOLIB_element_dType<T>::value);
      bufParamsGain.dim_x     = static_cast<uint32_t>(_numChannels);

      /******************************************************************************/
      /* Initialize the internal stages and set their SE/SA params                  */
      /******************************************************************************/

      AUDIOLIB_dB20_InitArgs kerInitArgs_dB20;
      kerInitArgs_dB20.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;

      AUDIOLIB_undB20_InitArgs kerInitArgs_undB20;
      kerInitArgs_undB20.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;

      const uint32_t handleSize_abs          = sizeof(__SE_TEMPLATE_v1) + sizeof(__SA_TEMPLATE_v1);
      const uint32_t handleSize_mul          = handleSize_abs + sizeof(__SE_TEMPLATE_v1);
      const uint32_t handleSize_gainComputer = sizeof(__SE_TEMPLATE_v1) + 2 * sizeof(__SA_TEMPLATE_v1);

      _handle_abs          = make_unique_handle<char>(handleSize_abs);
      _handle_dB20         = make_unique_handle<char>(AUDIOLIB_dB20_getHandleSize(&kerInitArgs_dB20));
      _handle_gainComputer = make_unique_handle<char>(handleSize_gainComputer);
      _handle_undB20       = make_unique_handle<char>(AUDIOLIB_undB20_getHandleSize(&kerInitArgs_undB20));
      _handle_mul          = make_unique_handle<char>(handleSize_mul);

      if (!_handle_abs || !_handle_dB20 || !_handle_gainComputer || !_handle_undB20 || !_handle_mul) {
         errorCtx.setMemoryError("DownwardExpander constructor", "failed to allocate downward expander handles");
      }
      else if (errorCtx.remapXLIBError(TISP_downwardExpander_abs_init(_handle_abs.get(), &bufParamsIn, &bufParamsOut),
                                       "TISP_downwardExpander_abs_init") &&
               errorCtx.remapXLIBError(
                   AUDIOLIB_dB20_init(_handle_dB20.get(), &bufParamsOut, &bufParamsScratch, &kerInitArgs_dB20),
                   "AUDIOLIB_dB20_init") &&
               errorCtx.remapXLIBError(TISP_downwardExpander_gainComputer_init(_handle_gainComputer.get(),
                                                                               &bufParamsScratch, &bufParamsGain,
                                                                               &bufParamsOut),
                                       "TISP_downwardExpander_gainComputer_init") &&
               errorCtx.remapXLIBError(
                   AUDIOLIB_undB20_init(_handle_undB20.get(), &bufParamsOut, &bufParamsScratch, &kerInitArgs_undB20),
                   "AUDIOLIB_undB20_init") &&
               errorCtx.remapXLIBError(
                   TISP_downwardExpander_mul_init(_handle_mul.get(), &bufParamsIn, &bufParamsScratch, &bufParamsOut),
                   "TISP_downwardExpander_mul_init")) {
         cleanInternalBuffers();
      }
      else {
         /* Nothing to do here */
      }
   }
}

template <typename T> void *DownwardExpander<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *DownwardExpander<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t DownwardExpander<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t DownwardExpander<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::DownwardExpander setAddr]
template <typename T> void DownwardExpander<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addr.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T>
bool DownwardExpander<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   const size_t arrayBytes = _numChannels * sizeof(T);

   DownwardExpanderTunableParams<T> params;
   params.pThresholdDb  = _thresholdDb;
   params.pRatio        = _ratio;
   params.pKneeWidthDb  = _kneeWidthDb;
   params.pAttackCoeff  = _attackCoeff;
   params.pReleaseCoeff = _releaseCoeff;
   params.pHoldSamples  = _holdSamples;
   params.bypassMode    = _bypassMode;

   if (value == nullptr) {
      errorCtx.setNullPointerError("DownwardExpander::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<DownwardExpanderParamId>(paramId)) {
      case DownwardExpanderParamId::kBypassMode:
         if (size >= sizeof(uint32_t)) {
            (void) memcpy(&params.bypassMode, value, sizeof(uint32_t));
         }
         break;
      case DownwardExpanderParamId::kThresholdDb:
         params.pThresholdDb = static_cast<T *>(value);
         break;
      case DownwardExpanderParamId::kRatio:
         params.pRatio = static_cast<T *>(value);
         break;
      case DownwardExpanderParamId::kKneeWidthDb:
         params.pKneeWidthDb = static_cast<T *>(value);
         break;
      case DownwardExpanderParamId::kAttackCoeff:
         params.pAttackCoeff = static_cast<T *>(value);
         break;
      case DownwardExpanderParamId::kReleaseCoeff:
         params.pReleaseCoeff = static_cast<T *>(value);
         break;
      case DownwardExpanderParamId::kHoldSamples:
         params.pHoldSamples = static_cast<T *>(value);
         break;
      case DownwardExpanderParamId::kAll:
         if (size >= sizeof(DownwardExpanderTunableParams<T>)) {
            params = *static_cast<const DownwardExpanderTunableParams<T> *>(value);
         }
         break;
      default:
         errorCtx.setInvalidParameterError("DownwardExpander::setParams", "Invalid paramId");
         break;
      }
   }

   if (errorCtx.isSuccess()) {
      const DownwardExpanderParamId id = static_cast<DownwardExpanderParamId>(paramId);
      const size_t                  expectedSize =
          (id == DownwardExpanderParamId::kBypassMode)
              ? sizeof(uint32_t)
              : ((id == DownwardExpanderParamId::kAll) ? sizeof(DownwardExpanderTunableParams<T>) : arrayBytes);

      if (size < expectedSize) {
         errorCtx.setInvalidParameterError("DownwardExpander::setParams", "Value size smaller than expected");
      }
      else if ((params.pThresholdDb == nullptr) || (params.pRatio == nullptr) || (params.pKneeWidthDb == nullptr) ||
               (params.pAttackCoeff == nullptr) || (params.pReleaseCoeff == nullptr) ||
               (params.pHoldSamples == nullptr)) {
         errorCtx.setNullPointerError(
             "DownwardExpander::setParams",
             "threshold, ratio, knee width, attack, release, and hold arrays must be non-null");
      }
      else if (params.bypassMode > 1U) {
         errorCtx.setInvalidParameterError("DownwardExpander::setParams", "bypass mode must be either zero or one");
      }
      else {
         // Per-channel range checks on the candidate values; nothing is written back if one fails
         for (size_t i = 0; (i < _numChannels) && errorCtx.isSuccess(); i++) {
            if (params.pThresholdDb[i] > static_cast<T>(0.0)) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander::setParams", "kThresholdDb must be <= 0.0");
            }
            else if (params.pRatio[i] < static_cast<T>(1.0)) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander::setParams", "kRatio must be >= 1.0");
            }
            else if (params.pKneeWidthDb[i] < static_cast<T>(0.0)) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander::setParams", "kKneeWidthDb must be >= 0.0");
            }
            else if ((params.pAttackCoeff[i] < static_cast<T>(0.0)) || // NOLINT(MISRA.PTR.ARITH)
                     (params.pAttackCoeff[i] > static_cast<T>(1.0))) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander::setParams",
                                                 "kAttackCoeff coefficients must be between 0.0 and 1.0");
            }
            else if ((params.pReleaseCoeff[i] < static_cast<T>(0.0)) || // NOLINT(MISRA.PTR.ARITH)
                     (params.pReleaseCoeff[i] > static_cast<T>(1.0))) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander::setParams",
                                                 "kReleaseCoeff coefficients must be between 0.0 and 1.0");
            }
            else if ((params.pHoldSamples[i] < static_cast<T>(0.0)) ||        // NOLINT(MISRA.PTR.ARITH)
                     (params.pHoldSamples[i] > static_cast<T>(16777215.0))) { // NOLINT(MISRA.PTR.ARITH)
               errorCtx.setInvalidParameterError("DownwardExpander::setParams",
                                                 "kHoldSamples must be between 0.0 and 16777215.0");
            }
            else {
               /* Nothing to do here */
            }
         }
      }

      if (errorCtx.isSuccess()) {
         if (params.pThresholdDb != _thresholdDb) {
            (void) memcpy(_thresholdDb, params.pThresholdDb, arrayBytes);
         }
         if (params.pRatio != _ratio) {
            (void) memcpy(_ratio, params.pRatio, arrayBytes);
         }
         if (params.pKneeWidthDb != _kneeWidthDb) {
            (void) memcpy(_kneeWidthDb, params.pKneeWidthDb, arrayBytes);
         }
         if (params.pAttackCoeff != _attackCoeff) {
            (void) memcpy(_attackCoeff, params.pAttackCoeff, arrayBytes);
         }
         if (params.pReleaseCoeff != _releaseCoeff) {
            (void) memcpy(_releaseCoeff, params.pReleaseCoeff, arrayBytes);
         }
         if (params.pHoldSamples != _holdSamples) {
            (void) memcpy(_holdSamples, params.pHoldSamples, arrayBytes);
         }
         // Leaving bypass: the gain envelope and hold counter are stale, so resume from rest
         if ((_bypassMode != 0U) && (params.bypassMode == 0U)) {
            cleanInternalBuffers();
         }
         _bypassMode = params.bypassMode;
      }
      else {
         /* Nothing to do here */
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool DownwardExpander<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   const size_t arrayBytes   = _numChannels * sizeof(T);
   size_t       expectedSize = arrayBytes;
   const void  *pSrc         = nullptr;

   if (size == nullptr) {
      errorCtx.setNullPointerError("DownwardExpander::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<DownwardExpanderParamId>(paramId)) {
      case DownwardExpanderParamId::kBypassMode:
         expectedSize = sizeof(uint32_t);
         pSrc         = &_bypassMode;
         break;
      case DownwardExpanderParamId::kThresholdDb:
         pSrc = _thresholdDb;
         break;
      case DownwardExpanderParamId::kRatio:
         pSrc = _ratio;
         break;
      case DownwardExpanderParamId::kKneeWidthDb:
         pSrc = _kneeWidthDb;
         break;
      case DownwardExpanderParamId::kAttackCoeff:
         pSrc = _attackCoeff;
         break;
      case DownwardExpanderParamId::kReleaseCoeff:
         pSrc = _releaseCoeff;
         break;
      case DownwardExpanderParamId::kHoldSamples:
         pSrc = _holdSamples;
         break;
      case DownwardExpanderParamId::kAll:
         expectedSize = sizeof(DownwardExpanderTunableParams<T>);
         break;
      default:
         errorCtx.setInvalidParameterError("DownwardExpander::getParams", "Invalid paramId");
         break;
      }
   }

   if (errorCtx.isSuccess()) {
      if (value == nullptr) {
         errorCtx.setNullPointerError("DownwardExpander::getParams", "value pointer is null");
      }
      else if (*size < expectedSize) {
         errorCtx.setInvalidParameterError("DownwardExpander::getParams", "Value size smaller than expected");
      }
      else if (static_cast<DownwardExpanderParamId>(paramId) == DownwardExpanderParamId::kAll) {
         DownwardExpanderTunableParams<T> *pParams = static_cast<DownwardExpanderTunableParams<T> *>(value);
         if ((pParams->pThresholdDb == nullptr) || (pParams->pRatio == nullptr) || (pParams->pKneeWidthDb == nullptr) ||
             (pParams->pAttackCoeff == nullptr) || (pParams->pReleaseCoeff == nullptr) ||
             (pParams->pHoldSamples == nullptr)) {
            errorCtx.setNullPointerError(
                "DownwardExpander::getParams",
                "threshold, ratio, knee width, attack, release, and hold arrays must be non-null");
         }
         else {
            if (pParams->pThresholdDb != _thresholdDb) {
               (void) memcpy(pParams->pThresholdDb, _thresholdDb, arrayBytes);
            }
            if (pParams->pRatio != _ratio) {
               (void) memcpy(pParams->pRatio, _ratio, arrayBytes);
            }
            if (pParams->pKneeWidthDb != _kneeWidthDb) {
               (void) memcpy(pParams->pKneeWidthDb, _kneeWidthDb, arrayBytes);
            }
            if (pParams->pAttackCoeff != _attackCoeff) {
               (void) memcpy(pParams->pAttackCoeff, _attackCoeff, arrayBytes);
            }
            if (pParams->pReleaseCoeff != _releaseCoeff) {
               (void) memcpy(pParams->pReleaseCoeff, _releaseCoeff, arrayBytes);
            }
            if (pParams->pHoldSamples != _holdSamples) {
               (void) memcpy(pParams->pHoldSamples, _holdSamples, arrayBytes);
            }
            pParams->bypassMode = _bypassMode;
         }
      }
      else if (value != pSrc) {
         (void) memcpy(value, pSrc, expectedSize);
      }
      else {
         /* Nothing to do here */
      }
      *size = expectedSize;
   }

   return errorCtx.isSuccess();
}

template <typename T> void DownwardExpander<T>::cleanInternalBuffers() noexcept
{
   if (_gainState != nullptr) {
      (void) memset(_gainState, 0, _numChannels * sizeof(T));
   }
   if (_holdCounter != nullptr) {
      (void) memset(_holdCounter, 0, _numChannels * sizeof(T));
   }
}

template <typename T> void DownwardExpander<T>::bypassCopy() noexcept
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

template <typename T> void DownwardExpander<T>::exec() noexcept
{
   if (_bypassMode != 0U) {
      bypassCopy();
   }
   else {
      T *pX = static_cast<T *>(_in.pBuf);
      T *pY = static_cast<T *>(_out.pBuf);

      TISP_downwardExpander_abs_exec(_handle_abs.get(), pX, pY);
      AUDIOLIB_dB20_exec(_handle_dB20.get(), pY, _scratch);

      TISP_downwardExpander_gainComputer_exec(_handle_gainComputer.get(), _scratch, pY);
      AUDIOLIB_undB20_exec(_handle_undB20.get(), pY, _scratch);
     
      TISP_downwardExpander_mul_exec(_handle_mul.get(), pX, _scratch, pY);
   }
}

template <typename T>
int32_t DownwardExpander<T>::TISP_downwardExpander_abs_init(void                   *handle,
                                                            AUDIOLIB_bufParams2D_t *bufParamsIn,
                                                            AUDIOLIB_bufParams2D_t *bufParamsOut)
{
   int32_t status = 0;
   // Setup SE/SA for abs operation
   __SE_TEMPLATE_v1 se0Params = __gen_SE_TEMPLATE_v1();
   __SA_TEMPLATE_v1 sa0Params = __gen_SA_TEMPLATE_v1();

   using vec = typename c7x::make_full_vector<T>::type;

   se0Params.DIMFMT  = __SE_DIMFMT_2D;
   se0Params.VECLEN  = c7x::se_veclen<vec>::value;
   se0Params.ELETYPE = c7x::se_eletype<vec>::value;

   se0Params.ICNT0 = static_cast<uint32_t>(bufParamsIn->dim_x);
   se0Params.DIM1  = static_cast<int32_t>(bufParamsIn->stride_y / sizeof(T));
   se0Params.ICNT1 = static_cast<uint32_t>(bufParamsIn->dim_y);

   sa0Params.DIMFMT = __SA_DIMFMT_2D;
   sa0Params.VECLEN = c7x::sa_veclen<vec>::value;

   sa0Params.ICNT0 = static_cast<uint32_t>(bufParamsOut->dim_x);
   sa0Params.DIM1  = static_cast<int32_t>(bufParamsOut->stride_y / sizeof(T));
   sa0Params.ICNT1 = static_cast<uint32_t>(bufParamsOut->dim_y);

   // Store parameters to handle
   uint8_t *handle_abs                        = static_cast<uint8_t *>(handle);
   *(static_cast<__SE_TEMPLATE_v1 *>(handle)) = se0Params;
   *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_abs + sizeof(__SE_TEMPLATE_v1)))) = sa0Params;

   return status;
}

template <typename T>
int32_t DownwardExpander<T>::TISP_downwardExpander_mul_init(void                   *handle,
                                                            AUDIOLIB_bufParams2D_t *bufParamsIn0,
                                                            AUDIOLIB_bufParams2D_t *bufParamsIn1,
                                                            AUDIOLIB_bufParams2D_t *bufParamsOut)
{
   int32_t status = 0;
   // Setup SE/SA for multiply operation
   __SE_TEMPLATE_v1 se0Params = __gen_SE_TEMPLATE_v1();
   __SE_TEMPLATE_v1 se1Params = __gen_SE_TEMPLATE_v1();
   __SA_TEMPLATE_v1 sa0Params = __gen_SA_TEMPLATE_v1();

   using vec = typename c7x::make_full_vector<T>::type;

   se0Params.DIMFMT  = __SE_DIMFMT_2D;
   se0Params.VECLEN  = c7x::se_veclen<vec>::value;
   se0Params.ELETYPE = c7x::se_eletype<vec>::value;

   se0Params.ICNT0 = static_cast<uint32_t>(bufParamsIn0->dim_x);
   se0Params.DIM1  = static_cast<int32_t>(bufParamsIn0->stride_y / sizeof(T));
   se0Params.ICNT1 = static_cast<uint32_t>(bufParamsIn0->dim_y);

   se1Params.DIMFMT  = __SE_DIMFMT_2D;
   se1Params.VECLEN  = c7x::se_veclen<vec>::value;
   se1Params.ELETYPE = c7x::se_eletype<vec>::value;

   se1Params.ICNT0 = static_cast<uint32_t>(bufParamsIn1->dim_x);
   se1Params.DIM1  = static_cast<int32_t>(bufParamsIn1->stride_y / sizeof(T));
   se1Params.ICNT1 = static_cast<uint32_t>(bufParamsIn1->dim_y);

   sa0Params.DIMFMT = __SA_DIMFMT_2D;
   sa0Params.VECLEN = c7x::sa_veclen<vec>::value;

   sa0Params.ICNT0 = static_cast<uint32_t>(bufParamsOut->dim_x);
   sa0Params.DIM1  = static_cast<int32_t>(bufParamsOut->stride_y / sizeof(T));
   sa0Params.ICNT1 = static_cast<uint32_t>(bufParamsOut->dim_y);

   // Store parameters to handle
   uint8_t *handle_mul                        = static_cast<uint8_t *>(handle);
   *(static_cast<__SE_TEMPLATE_v1 *>(handle)) = se0Params;
   *(static_cast<__SE_TEMPLATE_v1 *>(static_cast<void *>(handle_mul + sizeof(__SE_TEMPLATE_v1))))     = se1Params;
   *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_mul + 2 * sizeof(__SE_TEMPLATE_v1)))) = sa0Params;

   return status;
}

__INLINE_FUNC(TISP_downwardExpander_abs_exec)
template <typename T> inline void DownwardExpander<T>::TISP_downwardExpander_abs_exec(void *handle, T *pIn, T *pOut)
{
   // Extract SE/SA templates
   uint8_t         *handle_abs = static_cast<uint8_t *>(handle);
   __SE_TEMPLATE_v1 se0Params  = *(static_cast<__SE_TEMPLATE_v1 *>(handle));
   __SA_TEMPLATE_v1 sa0Params =
       *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_abs + sizeof(__SE_TEMPLATE_v1))));

   using vec        = typename c7x::make_full_vector<T>::type;
   uint32_t vecSize = c7x::element_count_of<vec>::value;

   // flatten 2D structure into 1D
   uint32_t numBlocks = static_cast<uint32_t>(this->_blockSize * AUDIOLIB_ceilingDiv(this->_numChannels, vecSize));

   // Define local buffer pointers
   T *pInLocal  = pIn;
   T *pOutLocal = pOut;

   // Open SE/SA
   __SE0_OPEN(pInLocal, se0Params);
   __SA0_OPEN(sa0Params);

   for (uint32_t i = 0; i < numBlocks; i++) {

      vec vIn = c7x::strm_eng<0, vec>::get_adv();

      // MATLAB floors |x| at DBL_EPSILON, i.e. the level at 20*log10(DBL_EPSILON) = -313.0712 dB, so digital silence
      // gives a finite, bounded gain (AUDIOLIB's dB20 alone would floor at -758.6 dB). Done here, where the pass has
      // spare slots, so the gain-computer loop does not pay for it.
      vec vOut = __max(__abs(vIn), (vec) 2.220446049250313e-16); // NOLINT(MISRA.C_CAST)

      __vpred vPred = c7x::strm_agen<0, vec>::get_vpred();
      vec    *vAddr = c7x::strm_agen<0, vec>::get_adv(pOutLocal);
      __vstore_pred(vPred, vAddr, vOut);
   }

   // Close SE/SA
   __SE0_CLOSE();
   __SA0_CLOSE();

   return;
}

__INLINE_FUNC(TISP_downwardExpander_mul_exec)
template <typename T>
inline void DownwardExpander<T>::TISP_downwardExpander_mul_exec(void *handle, T *pIn0, T *pIn1, T *pOut)
{
   // Extract SE/SA templates
   uint8_t         *handle_mul = static_cast<uint8_t *>(handle);
   __SE_TEMPLATE_v1 se0Params  = *(static_cast<__SE_TEMPLATE_v1 *>(handle));
   __SE_TEMPLATE_v1 se1Params =
       *(static_cast<__SE_TEMPLATE_v1 *>(static_cast<void *>(handle_mul + sizeof(__SE_TEMPLATE_v1))));
   __SA_TEMPLATE_v1 sa0Params =
       *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_mul + 2 * sizeof(__SE_TEMPLATE_v1))));

   using vec        = typename c7x::make_full_vector<T>::type;
   uint32_t vecSize = c7x::element_count_of<vec>::value;

   // flatten 2D structure into 1D
   uint32_t numBlocks = static_cast<uint32_t>(this->_blockSize * AUDIOLIB_ceilingDiv(this->_numChannels, vecSize));

   // Define local buffer pointers
   T *pIn0Local = pIn0;
   T *pOutLocal = pOut;
   T *pIn1Local = pIn1;

   // Open SE/SA
   __SE0_OPEN(pIn0Local, se0Params);
   __SE1_OPEN(pIn1Local, se1Params);
   __SA0_OPEN(sa0Params);

   for (uint32_t i = 0; i < numBlocks; i++) {

      vec vIn0 = c7x::strm_eng<0, vec>::get_adv();
      vec vIn1 = c7x::strm_eng<1, vec>::get_adv();

      vec vOut = vIn0 * vIn1;

      __vpred vPred = c7x::strm_agen<0, vec>::get_vpred();
      vec    *vAddr = c7x::strm_agen<0, vec>::get_adv(pOutLocal);
      __vstore_pred(vPred, vAddr, vOut);
   }

   // Close SE/SA
   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();

   return;
}

template <typename T>
int32_t DownwardExpander<T>::TISP_downwardExpander_gainComputer_init(void                   *handle,
                                                                     AUDIOLIB_bufParams2D_t *bufParamsIn,
                                                                     AUDIOLIB_bufParams1D_t *bufParamsGain,
                                                                     AUDIOLIB_bufParams2D_t *bufParamsOut)
{
   int32_t          status    = 0;
   __SE_TEMPLATE_v1 se0Params = __gen_SE_TEMPLATE_v1();

   __SA_TEMPLATE_v1 sa0Params = __gen_SA_TEMPLATE_v1();
   __SA_TEMPLATE_v1 sa1Params = __gen_SA_TEMPLATE_v1();

   using vec           = typename c7x::make_full_vector<T>::type;
   size_t elementCount = c7x::element_count_of<vec>::value;

   // Fetch 2D input buffer with SE0
   se0Params.DIMFMT  = __SE_DIMFMT_3D;
   se0Params.VECLEN  = c7x::se_veclen<vec>::value;
   se0Params.ELETYPE = c7x::se_eletype<vec>::value;

   se0Params.ICNT0 = static_cast<uint32_t>(elementCount);
   se0Params.DIM1  = static_cast<int32_t>(bufParamsIn->stride_y / AUDIOLIB_sizeof(bufParamsIn->data_type));
   se0Params.ICNT1 = static_cast<uint32_t>(bufParamsIn->dim_y);
   se0Params.DIM2  = static_cast<int32_t>(elementCount);
   se0Params.ICNT2 = static_cast<uint32_t>(AUDIOLIB_ceilingDiv(bufParamsIn->dim_x, elementCount));

   se0Params.DECDIM1       = __SE_DECDIM_DIM2;
   se0Params.DECDIM1_WIDTH = static_cast<uint32_t>(bufParamsIn->dim_x);
   se0Params.DECDIM1SD     = __SE_DECDIMSD_DIM0;

   // Store 2D output buffer with SA0
   sa0Params.DIMFMT = __SA_DIMFMT_3D;
   sa0Params.VECLEN = c7x::sa_veclen<vec>::value;

   sa0Params.ICNT0 = static_cast<uint32_t>(elementCount);
   sa0Params.DIM1  = static_cast<int32_t>(bufParamsOut->stride_y / AUDIOLIB_sizeof(bufParamsOut->data_type));
   sa0Params.ICNT1 = static_cast<uint32_t>(bufParamsOut->dim_y);
   sa0Params.DIM2  = static_cast<int32_t>(elementCount);
   sa0Params.ICNT2 = static_cast<uint32_t>(AUDIOLIB_ceilingDiv(bufParamsOut->dim_x, elementCount));

   sa0Params.DECDIM1       = __SA_DECDIM_DIM2;
   sa0Params.DECDIM1_WIDTH = static_cast<uint32_t>(bufParamsOut->dim_x);
   sa0Params.DECDIM1SD     = __SA_DECDIMSD_DIM0;

   // Address the 1D per-channel state buffers with SA1 and the 1D per-channel parameter buffers with SA2
   // (Use same SA parameters for address generation pattern)
   sa1Params.DIMFMT = __SA_DIMFMT_1D;
   sa1Params.VECLEN = c7x::sa_veclen<vec>::value;

   sa1Params.ICNT0 = static_cast<uint32_t>(bufParamsGain->dim_x);

   uint8_t *handle_gainComputer               = static_cast<uint8_t *>(handle);
   *(static_cast<__SE_TEMPLATE_v1 *>(handle)) = se0Params;
   *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1)))) = sa0Params;
   *(static_cast<__SA_TEMPLATE_v1 *>(
       static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1) + sizeof(__SA_TEMPLATE_v1))))    = sa1Params;

   return status;
}

__INLINE_FUNC(TISP_downwardExpander_kneeParams_exec)
template <typename T>
template <typename vecType>
inline void DownwardExpander<T>::TISP_downwardExpander_kneeParams_exec(vecType  vThreshold,
                                                                       vecType  vRatio,
                                                                       vecType  vKneeWidth,
                                                                       vecType &vKneeLower,
                                                                       vecType &vKneeUpper,
                                                                       vecType &vSoftKneeCoeff,
                                                                       vecType &vRatioMinusOne)
{
   vecType vZero = (vecType) 0;   // NOLINT(MISRA.C_CAST)
   vecType vOne  = (vecType) 1;   // NOLINT(MISRA.C_CAST)
   vecType vHalf = (vecType) 0.5; // NOLINT(MISRA.C_CAST)

   // Hard-knee lanes (W <= 0) run through the same soft-knee formula with W = 0: the knee
   // collapses to S = E = T and its quadratic coefficient is forced to zero below.
   __vpred cmpHardKneePolicy = __cmp_le_pred(vKneeWidth, vZero);
   vKneeWidth                = __select(cmpHardKneePolicy, vZero, vKneeWidth);

   // Gain below the knee: x_sc - x = (R - 1) * (x - T)
   vRatioMinusOne = vRatio - vOne;

   // Soft Knee Regions: x_sc - x = (1 - R) * (x - T - W/2)^2 / (2W)
   vecType vKneeWidthHalfBand      = vKneeWidth * vHalf;      // M, N
   vecType vKneeWidthDoubleBand    = vKneeWidth + vKneeWidth; // L, S, C
   vecType vInvKneeWidthDoubleBand = DownwardExpander<T>::TISP_downwardExpander_div_exec<vecType>(vKneeWidthDoubleBand);
   vKneeLower                      = vThreshold - vKneeWidthHalfBand; // L, S, C
   vKneeUpper                      = vThreshold + vKneeWidthHalfBand; // L, S, C

   // vInvKneeWidthDoubleBand is 1/0 on hard-knee lanes; zero the coefficient there so the
   // quadratic term vanishes instead of producing NaN.
   vSoftKneeCoeff = __select(cmpHardKneePolicy, vZero, (vOne - vRatio) * vInvKneeWidthDoubleBand);
}

__INLINE_FUNC(TISP_downwardExpander_targetGain_vec)
template <typename T>
template <typename vecType>
inline vecType DownwardExpander<T>::TISP_downwardExpander_targetGain_vec(vecType vIn,
                                                                         vecType vKneeLower,
                                                                         vecType vKneeUpper,
                                                                         vecType vSoftKneeCoeff,
                                                                         vecType vRatioMinusOne)
{
   vecType vHalfWidth   = (vKneeUpper - vKneeLower) * (vecType) 0.5; // NOLINT(MISRA.C_CAST)
   vecType vFromUpper   = vIn - vKneeUpper;
   __vpred cmpBelowKnee = __cmp_lt_pred(vFromUpper, vKneeLower - vKneeUpper);
   vecType vKneeDist    = __min(vFromUpper, (vecType) 0); // NOLINT(MISRA.C_CAST)
   vecType vSlope       = __select(cmpBelowKnee, vRatioMinusOne, vKneeDist * vSoftKneeCoeff);
   vecType vDist        = __select(cmpBelowKnee, vFromUpper + vHalfWidth, vKneeDist);

   return vSlope * vDist;
}

__INLINE_FUNC(TISP_downwardExpander_smoothStep_vec)
template <typename T>
template <typename vecType>
inline vecType DownwardExpander<T>::TISP_downwardExpander_smoothStep_vec(vecType  vTargetGain,
                                                                         vecType  vSmoothedGain,
                                                                         vecType &vHoldCount,
                                                                         vecType  vAttackCoeff,
                                                                         vecType  vOneMinusAttackCoeff,
                                                                         vecType  vReleaseCoeff,
                                                                         vecType  vOneMinusReleaseCoeff,
                                                                         vecType  vHoldSamples)
{
   vecType vZero = (vecType) 0; // NOLINT(MISRA.C_CAST)
   vecType vOne  = (vecType) 1; // NOLINT(MISRA.C_CAST)

   __vpred cmpAttackMode = __cmp_le_pred(vTargetGain, vSmoothedGain);
   __vpred cmpBelow      = __cmp_lt_pred(vTargetGain, vSmoothedGain);

   vecType vHoldCountNext = vHoldCount + vOne;
   __vpred cmpHoldElapsed = __cmp_lt_pred(vHoldSamples, vHoldCountNext);
   vHoldCount             = __select(cmpBelow, vHoldCountNext, __select(cmpAttackMode, vHoldCount, vZero));

   // (VMPYSP 4 + VADDSP 3 + VSEL 1 = 8 cycles, instead of 9 with separate hold and release selects).
   vecType vAttackCoeffHold = __select(cmpHoldElapsed, vAttackCoeff, vOne);
   vecType vAttackDrive     = __select(cmpHoldElapsed, vOneMinusAttackCoeff * vTargetGain, vZero);

   // A release toward 0 dB stops at -FLT_MIN instead of flushing to 0, so the next 0 dB target resets the hold.
   vecType vReleaseFloor =
       __select(__cmp_lt_pred(vZero, vReleaseCoeff), (vecType) (-std::numeric_limits<T>::min()), vZero); // NOLINT(MISRA.C_CAST)
   vecType vReleaseDrive        = vOneMinusReleaseCoeff * vTargetGain + vReleaseFloor;
   vecType vSmoothedGainRelease = vReleaseCoeff * vSmoothedGain + vReleaseDrive;
   vecType vSmoothedGainAttack  = vAttackCoeffHold * vSmoothedGain + vAttackDrive;

   return __select(cmpAttackMode, vSmoothedGainAttack, vSmoothedGainRelease);
}

__INLINE_FUNC(TISP_downwardExpander_gainComputer_exec)
template <typename T>
inline void DownwardExpander<T>::TISP_downwardExpander_gainComputer_exec(void *handle, T *pIn, T *pOut)
{
   using vec             = typename c7x::make_full_vector<T>::type;
   uint32_t elementCount = c7x::element_count_of<vec>::value;

   const uint32_t numBlocks = static_cast<uint32_t>(AUDIOLIB_ceilingDiv(this->_numChannels, elementCount));
   const uint32_t numPairs  = numBlocks / 2U;

   // Channel blocks in pairs
   if (numPairs > 0U) {
      TISP_downwardExpander_targetGain_exec(handle, pIn, numPairs);
      TISP_downwardExpander_smoothPairs_exec(handle, pIn, pOut, numPairs);
   }

   // A single or leftover channel block: target gain and smoothing in one loop.
   if ((2U * numPairs) < numBlocks) {
      TISP_downwardExpander_gainFused_exec(handle, pIn, pOut, 2U * numPairs);
   }

   return;
}

__INLINE_FUNC(TISP_downwardExpander_targetGain_exec)
template <typename T>
inline void DownwardExpander<T>::TISP_downwardExpander_targetGain_exec(void *handle, T *pInOut, uint32_t numPairs)
{
   // Extract SE/SA params
   uint8_t         *handle_gainComputer = static_cast<uint8_t *>(handle);
   __SE_TEMPLATE_v1 se0Params           = *(static_cast<__SE_TEMPLATE_v1 *>(handle));
   __SA_TEMPLATE_v1 sa0Params =
       *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1))));
   __SA_TEMPLATE_v1 sa1Params = *(static_cast<__SA_TEMPLATE_v1 *>(
       static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1) + sizeof(__SA_TEMPLATE_v1))));

   const uint32_t numTargetBlocks = 2U * numPairs;

   // Read the levels of the paired channel blocks and store their target gains over them: the
   // store walks the scratch buffer exactly like the level fetch does.
   se0Params.ICNT2                 = numTargetBlocks;
   __SA_TEMPLATE_v1 saTargetParams = sa0Params;
   saTargetParams.DIM1             = se0Params.DIM1;
   saTargetParams.ICNT2            = numTargetBlocks;

   // Assign local buffer addresses
   T *pThresholdLocal = static_cast<T *>(this->_thresholdDb);
   T *pRatioLocal     = static_cast<T *>(this->_ratio);
   T *pKneeWidthLocal = static_cast<T *>(this->_kneeWidthDb);
   T *pInOutLocal     = static_cast<T *>(pInOut);

   using vec = typename c7x::make_full_vector<T>::type;

   // Local trip count (see TISP_downwardExpander_gainFused_exec)
   const size_t blockSize = this->_blockSize;

   // Open SE/SA
   __SE0_OPEN(pInOutLocal, se0Params);
   __SA0_OPEN(saTargetParams);
   __SA2_OPEN(sa1Params);

#pragma COALESCE_LOOP
   for (uint32_t ch = 0; ch < numTargetBlocks; ch++) {

      // Fetch per channel knee parameters (SA2 offset shared by the parameter buffers)
      __vpred vPredParam = c7x::strm_agen<2, vec>::get_vpred();
      vec     vThreshold = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get(pThresholdLocal));
      vec     vRatio     = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get(pRatioLocal));
      vec     vKneeWidth = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get_adv(pKneeWidthLocal));

      vec vKneeLower, vKneeUpper, vSoftKneeCoeff, vRatioMinusOne;
      DownwardExpander<T>::TISP_downwardExpander_kneeParams_exec<vec>(vThreshold, vRatio, vKneeWidth, vKneeLower,
                                                                      vKneeUpper, vSoftKneeCoeff, vRatioMinusOne);

#pragma MUST_ITERATE(1, 65535, )
      for (size_t n = 0; n < blockSize; n++) {

         // fetch input vector (in decibel)
         vec vIn = c7x::strm_eng<0, vec>::get_adv();

         vec vTargetGain = DownwardExpander<T>::TISP_downwardExpander_targetGain_vec<vec>(
             vIn, vKneeLower, vKneeUpper, vSoftKneeCoeff, vRatioMinusOne);

         // Store target gains over the levels
         __vpred vPredOut = c7x::strm_agen<0, vec>::get_vpred();
         vec    *vAddrOut = c7x::strm_agen<0, vec>::get_adv(pInOutLocal);
         __vstore_pred(vPredOut, vAddrOut, vTargetGain);
      }
   }

   __SE0_CLOSE();
   __SA0_CLOSE();
   __SA2_CLOSE();

   return;
}

__INLINE_FUNC(TISP_downwardExpander_smoothPairs_exec)
template <typename T>
inline void
DownwardExpander<T>::TISP_downwardExpander_smoothPairs_exec(void *handle, T *pIn, T *pOut, uint32_t numPairs)
{
   // Extract SE/SA params
   uint8_t         *handle_gainComputer = static_cast<uint8_t *>(handle);
   __SE_TEMPLATE_v1 se0Params           = *(static_cast<__SE_TEMPLATE_v1 *>(handle));
   __SA_TEMPLATE_v1 sa0Params =
       *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1))));
   __SA_TEMPLATE_v1 sa1Params = *(static_cast<__SA_TEMPLATE_v1 *>(
       static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1) + sizeof(__SA_TEMPLATE_v1))));

   using vec             = typename c7x::make_full_vector<T>::type;
   uint32_t elementCount = c7x::element_count_of<vec>::value;

   // Block A of a pair is an even channel block, block B the odd one after it: both walk the
   // channel blocks with a two-block stride. Only a B block can be the partial last block, so
   // only its DECDIM width (counted from its own start) differs.
   __SE_TEMPLATE_v1 seAParams = se0Params;
   seAParams.DIM2             = static_cast<int32_t>(2U * elementCount);
   seAParams.ICNT2            = numPairs;
   __SE_TEMPLATE_v1 seBParams = seAParams;
   seBParams.DECDIM1_WIDTH    = static_cast<uint32_t>(this->_numChannels - elementCount);

   __SA_TEMPLATE_v1 saAParams = sa0Params;
   saAParams.DIM2             = static_cast<int32_t>(2U * elementCount);
   saAParams.ICNT2            = numPairs;
   __SA_TEMPLATE_v1 saBParams = saAParams;
   saBParams.DECDIM1_WIDTH    = static_cast<uint32_t>(this->_numChannels - elementCount);

   // Assign local buffer addresses
   T *pAttackCoeffLocal  = static_cast<T *>(this->_attackCoeff);
   T *pReleaseCoeffLocal = static_cast<T *>(this->_releaseCoeff);
   T *pHoldSamplesLocal  = static_cast<T *>(this->_holdSamples);
   T *pGainStateLocal    = static_cast<T *>(this->_gainState);
   T *pHoldStateLocal    = static_cast<T *>(this->_holdCounter);

   // Block B starts one channel block in: SE1 and SA3 offsets are relative to these bases
   T *pInLocal   = static_cast<T *>(pIn);
   T *pOutLocalA = static_cast<T *>(pOut);
   T *pOutLocalB = static_cast<T *>(pOut) + elementCount;

   // Local trip count (see TISP_downwardExpander_gainFused_exec)
   const size_t blockSize = this->_blockSize;

   vec vOne = (vec) 1; // NOLINT(MISRA.C_CAST)

   // Open SE/SA
   __SE0_OPEN(pInLocal, seAParams);
   __SE1_OPEN(pInLocal + elementCount, seBParams);
   __SA0_OPEN(saAParams);
   __SA3_OPEN(saBParams);
   __SA1_OPEN(sa1Params);
   __SA2_OPEN(sa1Params);

   for (uint32_t pair = 0; pair < numPairs; pair++) {

      // Fetch per channel smoothing parameters of block A, then of block B (SA2 advances once per block)
      __vpred vPredParamA    = c7x::strm_agen<2, vec>::get_vpred();
      vec     vAttackCoeffA  = __vload_pred(vPredParamA, c7x::strm_agen<2, vec>::get(pAttackCoeffLocal));
      vec     vReleaseCoeffA = __vload_pred(vPredParamA, c7x::strm_agen<2, vec>::get(pReleaseCoeffLocal));
      vec     vHoldSamplesA  = __vload_pred(vPredParamA, c7x::strm_agen<2, vec>::get_adv(pHoldSamplesLocal));

      __vpred vPredParamB    = c7x::strm_agen<2, vec>::get_vpred();
      vec     vAttackCoeffB  = __vload_pred(vPredParamB, c7x::strm_agen<2, vec>::get(pAttackCoeffLocal));
      vec     vReleaseCoeffB = __vload_pred(vPredParamB, c7x::strm_agen<2, vec>::get(pReleaseCoeffLocal));
      vec     vHoldSamplesB  = __vload_pred(vPredParamB, c7x::strm_agen<2, vec>::get_adv(pHoldSamplesLocal));

      vec vOneMinusAttackCoeffA  = vOne - vAttackCoeffA;  // L, S, C
      vec vOneMinusReleaseCoeffA = vOne - vReleaseCoeffA; // L, S, C
      vec vOneMinusAttackCoeffB  = vOne - vAttackCoeffB;  // L, S, C
      vec vOneMinusReleaseCoeffB = vOne - vReleaseCoeffB; // L, S, C

      // Resume both blocks' envelopes and hold counters where the previous exec() left them.
      __vpred vPredStateA    = c7x::strm_agen<1, vec>::get_vpred();
      vec    *vAddrGainA     = c7x::strm_agen<1, vec>::get(pGainStateLocal);
      vec    *vAddrHoldA     = c7x::strm_agen<1, vec>::get_adv(pHoldStateLocal);
      __vpred vPredStateB    = c7x::strm_agen<1, vec>::get_vpred();
      vec    *vAddrGainB     = c7x::strm_agen<1, vec>::get(pGainStateLocal);
      vec    *vAddrHoldB     = c7x::strm_agen<1, vec>::get_adv(pHoldStateLocal);
      vec     vSmoothedGainA = __vload_pred(vPredStateA, vAddrGainA);
      vec     vHoldCountA    = __vload_pred(vPredStateA, vAddrHoldA);
      vec     vSmoothedGainB = __vload_pred(vPredStateB, vAddrGainB);
      vec     vHoldCountB    = __vload_pred(vPredStateB, vAddrHoldB);

#pragma MUST_ITERATE(1, 65535, )
      for (size_t n = 0; n < blockSize; n++) {

         // Two independent recursions: block A through SE0/SA0, block B through SE1/SA3
         vec vTargetGainA = c7x::strm_eng<0, vec>::get_adv();
         vSmoothedGainA   = DownwardExpander<T>::TISP_downwardExpander_smoothStep_vec<vec>(
             vTargetGainA, vSmoothedGainA, vHoldCountA, vAttackCoeffA, vOneMinusAttackCoeffA, vReleaseCoeffA,
             vOneMinusReleaseCoeffA, vHoldSamplesA);

         __vpred vPredOutA = c7x::strm_agen<0, vec>::get_vpred();
         vec    *vAddrOutA = c7x::strm_agen<0, vec>::get_adv(pOutLocalA);
         __vstore_pred(vPredOutA, vAddrOutA, vSmoothedGainA);

         vec vTargetGainB = c7x::strm_eng<1, vec>::get_adv();
         vSmoothedGainB   = DownwardExpander<T>::TISP_downwardExpander_smoothStep_vec<vec>(
             vTargetGainB, vSmoothedGainB, vHoldCountB, vAttackCoeffB, vOneMinusAttackCoeffB, vReleaseCoeffB,
             vOneMinusReleaseCoeffB, vHoldSamplesB);

         __vpred vPredOutB = c7x::strm_agen<3, vec>::get_vpred();
         vec    *vAddrOutB = c7x::strm_agen<3, vec>::get_adv(pOutLocalB);
         __vstore_pred(vPredOutB, vAddrOutB, vSmoothedGainB);
      }

      // Carry both blocks' final envelopes and hold counters to the next exec().
      __vstore_pred(vPredStateA, vAddrGainA, vSmoothedGainA);
      __vstore_pred(vPredStateA, vAddrHoldA, vHoldCountA);
      __vstore_pred(vPredStateB, vAddrGainB, vSmoothedGainB);
      __vstore_pred(vPredStateB, vAddrHoldB, vHoldCountB);
   }

   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();
   __SA1_CLOSE();
   __SA2_CLOSE();
   __SA3_CLOSE();

   return;
}

__INLINE_FUNC(TISP_downwardExpander_gainFused_exec)
template <typename T>
inline void
DownwardExpander<T>::TISP_downwardExpander_gainFused_exec(void *handle, T *pIn, T *pOut, uint32_t firstBlock)
{
   // Extract SE/SA params
   uint8_t         *handle_gainComputer = static_cast<uint8_t *>(handle);
   __SE_TEMPLATE_v1 se0Params           = *(static_cast<__SE_TEMPLATE_v1 *>(handle));
   __SA_TEMPLATE_v1 sa0Params =
       *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1))));
   __SA_TEMPLATE_v1 sa1Params = *(static_cast<__SA_TEMPLATE_v1 *>(
       static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1) + sizeof(__SA_TEMPLATE_v1))));

   using vec             = typename c7x::make_full_vector<T>::type;
   uint32_t elementCount = c7x::element_count_of<vec>::value;

   // Start the walk at firstBlock: shift the buffers and shrink the channel extent of every pattern.
   const uint32_t firstChannel = firstBlock * elementCount;
   const uint32_t numChannels  = static_cast<uint32_t>(this->_numChannels) - firstChannel;
   uint32_t       numBlocks    = static_cast<uint32_t>(AUDIOLIB_ceilingDiv(numChannels, elementCount));

   se0Params.ICNT2         = numBlocks;
   se0Params.DECDIM1_WIDTH = numChannels;
   sa0Params.ICNT2         = numBlocks;
   sa0Params.DECDIM1_WIDTH = numChannels;
   sa1Params.ICNT0         = numChannels;

   // Assign local buffer addresses
   T *pThresholdLocal    = static_cast<T *>(this->_thresholdDb) + firstChannel;
   T *pRatioLocal        = static_cast<T *>(this->_ratio) + firstChannel;
   T *pKneeWidthLocal    = static_cast<T *>(this->_kneeWidthDb) + firstChannel;
   T *pAttackCoeffLocal  = static_cast<T *>(this->_attackCoeff) + firstChannel;
   T *pReleaseCoeffLocal = static_cast<T *>(this->_releaseCoeff) + firstChannel;
   T *pHoldSamplesLocal  = static_cast<T *>(this->_holdSamples) + firstChannel;
   T *pGainStateLocal    = static_cast<T *>(this->_gainState) + firstChannel;
   T *pHoldStateLocal    = static_cast<T *>(this->_holdCounter) + firstChannel;

   T *pInLocal  = static_cast<T *>(pIn) + firstChannel;
   T *pOutLocal = static_cast<T *>(pOut) + firstChannel;

   // Local trip count: the sample loop stores through pOut, which the compiler cannot prove does not
   // alias this->_blockSize, so reading the member in the loop condition makes the loop irregular and
   // blocks software pipelining at the dependency bound.
   const size_t blockSize = this->_blockSize;

   // Open SE/SA
   __SE0_OPEN(pInLocal, se0Params);
   __SA0_OPEN(sa0Params);
   __SA1_OPEN(sa1Params);
   __SA2_OPEN(sa1Params);

   // Define constant vectors
   vec vOne = (vec) 1; // NOLINT(MISRA.C_CAST)

   // Iterate through channel blocks
#pragma COALESCE_LOOP
   for (uint32_t ch = 0; ch < numBlocks; ch++) {

      // Fetch per channel parameters. The parameter buffers live at independent addresses, so
      // SA2 supplies one channel-block offset that every buffer shares; only the last fetch
      // advances it to the next channel block.
      __vpred vPredParam    = c7x::strm_agen<2, vec>::get_vpred();
      vec     vThreshold    = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get(pThresholdLocal));
      vec     vRatio        = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get(pRatioLocal));
      vec     vKneeWidth    = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get(pKneeWidthLocal));
      vec     vAttackCoeff  = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get(pAttackCoeffLocal));
      vec     vReleaseCoeff = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get(pReleaseCoeffLocal));
      vec     vHoldSamples  = __vload_pred(vPredParam, c7x::strm_agen<2, vec>::get_adv(pHoldSamplesLocal));

      vec vOneMinusAttackCoeff  = vOne - vAttackCoeff;  // L, S, C
      vec vOneMinusReleaseCoeff = vOne - vReleaseCoeff; // L, S, C

      vec vKneeLower, vKneeUpper, vSoftKneeCoeff, vRatioMinusOne;
      DownwardExpander<T>::TISP_downwardExpander_kneeParams_exec<vec>(vThreshold, vRatio, vKneeWidth, vKneeLower,
                                                                      vKneeUpper, vSoftKneeCoeff, vRatioMinusOne);

      // Resume this channel-block's envelope and hold counter where the previous exec() left them.
      __vpred vPredState    = c7x::strm_agen<1, vec>::get_vpred();
      vec    *vAddrGain     = c7x::strm_agen<1, vec>::get(pGainStateLocal);
      vec    *vAddrHold     = c7x::strm_agen<1, vec>::get_adv(pHoldStateLocal);
      vec     vSmoothedGain = __vload_pred(vPredState, vAddrGain);
      vec     vHoldCount    = __vload_pred(vPredState, vAddrHold);

#pragma MUST_ITERATE(1, 65535, )
      for (size_t n = 0; n < blockSize; n++) {

         // fetch input vector (in decibel)
         vec vIn = c7x::strm_eng<0, vec>::get_adv();

         vec vTargetGain = DownwardExpander<T>::TISP_downwardExpander_targetGain_vec<vec>(
             vIn, vKneeLower, vKneeUpper, vSoftKneeCoeff, vRatioMinusOne);

         vSmoothedGain = DownwardExpander<T>::TISP_downwardExpander_smoothStep_vec<vec>(
             vTargetGain, vSmoothedGain, vHoldCount, vAttackCoeff, vOneMinusAttackCoeff, vReleaseCoeff,
             vOneMinusReleaseCoeff, vHoldSamples);

         // Store smoothed gains
         __vpred vPredOut = c7x::strm_agen<0, vec>::get_vpred();
         vec    *vAddrOut = c7x::strm_agen<0, vec>::get_adv(pOutLocal);
         __vstore_pred(vPredOut, vAddrOut, vSmoothedGain);
      }

      // Carry this block's final envelope and hold counter to the next exec().
      __vstore_pred(vPredState, vAddrGain, vSmoothedGain);
      __vstore_pred(vPredState, vAddrHold, vHoldCount);
   }

   __SE0_CLOSE();
   __SA0_CLOSE();
   __SA1_CLOSE();
   __SA2_CLOSE();

   return;
}

__INLINE_FUNC(TISP_downwardExpander_div_exec)
template <typename T>
template <typename vecType>
inline vecType DownwardExpander<T>::TISP_downwardExpander_div_exec(vecType vIn)
{

   vecType vTwo     = (vecType) 2; // NOLINT(MISRA.C_CAST)
   vecType vRecipIn = __recip(vIn);
   vecType vIter1   = vRecipIn * vIn;
   vecType vDiff    = vTwo - vIter1;
   vecType vInverse = vRecipIn * vDiff;

   return vInverse;
}

template class DownwardExpander<float>;

} // namespace AUDIOLIB
} // namespace TISP
