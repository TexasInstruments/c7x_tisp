// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <TISP_limiter.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Limiter constructor]

template <typename T>
Limiter<T>::Limiter(const TISP_BufDescList        *pInputs,
                    const TISP_BufDescList        *pOutputs,
                    const TISP_BufDescList        *pStates,
                    void                          *pScratch,
                    size_t                         scratchBytes,
                    const LimiterTunableParams<T> *pDefaultTunableParams,
                    const char                    *nodeName,
                    uint32_t                       nodeId,
                    ErrorCtxt                     &errorCtx)
    : _in{}, _out{}, _scratch(static_cast<T *>(pScratch)), _gainState(nullptr), _thresholdDb(nullptr),
      _kneeWidthDb(nullptr), _attackCoeffs(nullptr), _releaseCoeffs(nullptr), _numChannels(0U), _blockSize(0U),
      _bypassMode(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pScratch == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "scratch pointer pScratch is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output, and one gain state buffer
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Limiter constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Limiter constructor (outputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pStates, 1U, 1U, "Limiter constructor (states)", errorCtx)) {
      _in          = pInputs->pDesc[0];
      _out         = pOutputs->pDesc[0];
      _numChannels = _in.width;
      _blockSize   = _in.height;

      if ((_in.isInterleave != 1U) || (_out.isInterleave != 1U)) {
         errorCtx.setInvalidParameterError("Limiter constructor", "input and output must be interleaved");
      }
      else if ((_out.width != _in.width) || (_out.height != _in.height)) {
         errorCtx.setInvalidParameterError("Limiter constructor", "output width and height must match the input");
      }
      else if (pStates->pDesc[0].width != _numChannels) {
         errorCtx.setInvalidParameterError("Limiter constructor", "gain state width must equal the channel count");
      }
      else if (scratchBytes < (_numChannels * _blockSize * sizeof(T))) {
         errorCtx.setInvalidParameterError("Limiter constructor", "scratchBytes is smaller than channels x samples");
      }
      else {
         (void) checkTunableParams(*pDefaultTunableParams, "Limiter constructor", errorCtx);
      }
   }

   if (errorCtx.isSuccess()) {
      _gainState     = static_cast<T *>(pStates->pDesc[0].pBuf);
      _thresholdDb   = pDefaultTunableParams->pThresholdDb;
      _kneeWidthDb   = pDefaultTunableParams->pKneeWidthDb;
      _attackCoeffs  = pDefaultTunableParams->pAttackCoeffs;
      _releaseCoeffs = pDefaultTunableParams->pReleaseCoeffs;
      _bypassMode    = pDefaultTunableParams->bypassMode;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numChannels=%zu, blockSize=%zu",
                      _numChannels, _blockSize);
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
      const uint32_t handleSize_gainComputer = 2 * sizeof(__SE_TEMPLATE_v1) + 2 * sizeof(__SA_TEMPLATE_v1);

      _handle_abs          = make_unique_handle<char>(handleSize_abs);
      _handle_dB20         = make_unique_handle<char>(AUDIOLIB_dB20_getHandleSize(&kerInitArgs_dB20));
      _handle_gainComputer = make_unique_handle<char>(handleSize_gainComputer);
      _handle_undB20       = make_unique_handle<char>(AUDIOLIB_undB20_getHandleSize(&kerInitArgs_undB20));
      _handle_mul          = make_unique_handle<char>(handleSize_mul);

      if (!_handle_abs || !_handle_dB20 || !_handle_gainComputer || !_handle_undB20 || !_handle_mul) {
         errorCtx.setMemoryError("Limiter constructor", "failed to allocate limiter handles");
      }
      else if (errorCtx.remapXLIBError(TISP_limiter_abs_init(_handle_abs.get(), &bufParamsIn, &bufParamsOut),
                                       "TISP_limiter_abs_init") &&
               errorCtx.remapXLIBError(
                   AUDIOLIB_dB20_init(_handle_dB20.get(), &bufParamsOut, &bufParamsScratch, &kerInitArgs_dB20),
                   "AUDIOLIB_dB20_init") &&
               errorCtx.remapXLIBError(TISP_limiter_gainComputer_init(_handle_gainComputer.get(), &bufParamsScratch,
                                                                      &bufParamsGain, &bufParamsOut),
                                       "TISP_limiter_gainComputer_init") &&
               errorCtx.remapXLIBError(
                   AUDIOLIB_undB20_init(_handle_undB20.get(), &bufParamsOut, &bufParamsScratch, &kerInitArgs_undB20),
                   "AUDIOLIB_undB20_init") &&
               errorCtx.remapXLIBError(
                   TISP_limiter_mul_init(_handle_mul.get(), &bufParamsIn, &bufParamsScratch, &bufParamsOut),
                   "TISP_limiter_mul_init")) {
         cleanInternalBuffers();
      }
      else {
         /* Nothing to do here */
      }
   }
}

template <typename T>
bool Limiter<T>::checkRange(const T    *pValues,
                            T           minValue,
                            T           maxValue,
                            const char *funcName,
                            const char *message,
                            ErrorCtxt  &errorCtx) const noexcept
{
   for (size_t i = 0; (i < _numChannels) && errorCtx.isSuccess(); i++) {
      if ((pValues[i] < minValue) || (pValues[i] > maxValue)) { // NOLINT(MISRA.PTR.ARITH)
         errorCtx.setInvalidParameterError(funcName, message);
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool Limiter<T>::checkTunableParams(const LimiterTunableParams<T> &params,
                                    const char                    *funcName,
                                    ErrorCtxt                     &errorCtx) noexcept
{
   const T lowest  = -std::numeric_limits<T>::max();
   const T highest = std::numeric_limits<T>::max();

   if ((params.pThresholdDb == nullptr) || (params.pKneeWidthDb == nullptr) || (params.pAttackCoeffs == nullptr) ||
       (params.pReleaseCoeffs == nullptr)) {
      errorCtx.setNullPointerError(funcName, "threshold, knee width, attack, and release arrays must be non-null");
   }
   else if (params.bypassMode > 1U) {
      errorCtx.setInvalidParameterError(funcName, "bypass mode must be either zero or one");
   }
   else if (checkRange(params.pThresholdDb, lowest, static_cast<T>(0.0), funcName, "kThreshold must be <= 0.0",
                       errorCtx) &&
            checkRange(params.pKneeWidthDb, static_cast<T>(0.0), highest, funcName, "kKneeWidth must be >= 0.0",
                       errorCtx) &&
            checkRange(params.pAttackCoeffs, static_cast<T>(0.0), static_cast<T>(1.0), funcName,
                       "kAttack coefficients must be between 0.0 and 1.0", errorCtx)) {
      (void) checkRange(params.pReleaseCoeffs, static_cast<T>(0.0), static_cast<T>(1.0), funcName,
                        "kRelease coefficients must be between 0.0 and 1.0", errorCtx);
   }
   else {
      /* Nothing to do here */
   }

   return errorCtx.isSuccess();
}

template <typename T> void *Limiter<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *Limiter<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Limiter<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Limiter<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Limiter setAddr]
template <typename T> void Limiter<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addr.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T>
bool Limiter<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   const size_t arrayBytes = _numChannels * sizeof(T);

   LimiterTunableParams<T> params;
   params.pThresholdDb   = _thresholdDb;
   params.pKneeWidthDb   = _kneeWidthDb;
   params.pAttackCoeffs  = _attackCoeffs;
   params.pReleaseCoeffs = _releaseCoeffs;
   params.bypassMode     = _bypassMode;

   if (value == nullptr) {
      errorCtx.setNullPointerError("Limiter::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<LimiterParamId>(paramId)) {
      case LimiterParamId::kThreshold:
         params.pThresholdDb = static_cast<T *>(value);
         break;
      case LimiterParamId::kKneeWidth:
         params.pKneeWidthDb = static_cast<T *>(value);
         break;
      case LimiterParamId::kAttack:
         params.pAttackCoeffs = static_cast<T *>(value);
         break;
      case LimiterParamId::kRelease:
         params.pReleaseCoeffs = static_cast<T *>(value);
         break;
      case LimiterParamId::kBypass:
         if (size >= sizeof(uint32_t)) {
            (void) memcpy(&params.bypassMode, value, sizeof(uint32_t));
         }
         break;
      case LimiterParamId::kAll:
         if (size >= sizeof(LimiterTunableParams<T>)) {
            params = *static_cast<const LimiterTunableParams<T> *>(value);
         }
         break;
      default:
         errorCtx.setInvalidParameterError("Limiter::setParams", "Invalid paramId");
         break;
      }
   }

   if (errorCtx.isSuccess()) {
      const LimiterParamId id = static_cast<LimiterParamId>(paramId);
      const size_t         expectedSize =
          (id == LimiterParamId::kBypass) ? sizeof(uint32_t)
                  : ((id == LimiterParamId::kAll) ? sizeof(LimiterTunableParams<T>) : arrayBytes);

      if (size < expectedSize) {
         errorCtx.setInvalidParameterError("Limiter::setParams", "Value size smaller than expected");
      }
      else if (checkTunableParams(params, "Limiter::setParams", errorCtx)) {
         if (params.pThresholdDb != _thresholdDb) {
            (void) memcpy(_thresholdDb, params.pThresholdDb, arrayBytes);
         }
         if (params.pKneeWidthDb != _kneeWidthDb) {
            (void) memcpy(_kneeWidthDb, params.pKneeWidthDb, arrayBytes);
         }
         if (params.pAttackCoeffs != _attackCoeffs) {
            (void) memcpy(_attackCoeffs, params.pAttackCoeffs, arrayBytes);
         }
         if (params.pReleaseCoeffs != _releaseCoeffs) {
            (void) memcpy(_releaseCoeffs, params.pReleaseCoeffs, arrayBytes);
         }
         // Leaving bypass: the gain envelope is stale, so resume from rest
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
bool Limiter<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   const size_t arrayBytes   = _numChannels * sizeof(T);
   size_t       expectedSize = arrayBytes;
   const void  *pSrc         = nullptr;

   if (size == nullptr) {
      errorCtx.setNullPointerError("Limiter::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<LimiterParamId>(paramId)) {
      case LimiterParamId::kThreshold:
         pSrc = _thresholdDb;
         break;
      case LimiterParamId::kKneeWidth:
         pSrc = _kneeWidthDb;
         break;
      case LimiterParamId::kAttack:
         pSrc = _attackCoeffs;
         break;
      case LimiterParamId::kRelease:
         pSrc = _releaseCoeffs;
         break;
      case LimiterParamId::kBypass:
         expectedSize = sizeof(uint32_t);
         pSrc         = &_bypassMode;
         break;
      case LimiterParamId::kAll:
         expectedSize = sizeof(LimiterTunableParams<T>);
         break;
      default:
         errorCtx.setInvalidParameterError("Limiter::getParams", "Invalid paramId");
         break;
      }
   }

   if (errorCtx.isSuccess()) {
      if (value == nullptr) {
         errorCtx.setNullPointerError("Limiter::getParams", "value pointer is null");
      }
      else if (*size < expectedSize) {
         errorCtx.setInvalidParameterError("Limiter::getParams", "Value size smaller than expected");
      }
      else if (static_cast<LimiterParamId>(paramId) == LimiterParamId::kAll) {
         LimiterTunableParams<T> *pParams = static_cast<LimiterTunableParams<T> *>(value);
         if ((pParams->pThresholdDb == nullptr) || (pParams->pKneeWidthDb == nullptr) ||
             (pParams->pAttackCoeffs == nullptr) || (pParams->pReleaseCoeffs == nullptr)) {
            errorCtx.setNullPointerError("Limiter::getParams",
                                         "threshold, knee width, attack, and release arrays must be non-null");
         }
         else {
            if (pParams->pThresholdDb != _thresholdDb) {
               (void) memcpy(pParams->pThresholdDb, _thresholdDb, arrayBytes);
            }
            if (pParams->pKneeWidthDb != _kneeWidthDb) {
               (void) memcpy(pParams->pKneeWidthDb, _kneeWidthDb, arrayBytes);
            }
            if (pParams->pAttackCoeffs != _attackCoeffs) {
               (void) memcpy(pParams->pAttackCoeffs, _attackCoeffs, arrayBytes);
            }
            if (pParams->pReleaseCoeffs != _releaseCoeffs) {
               (void) memcpy(pParams->pReleaseCoeffs, _releaseCoeffs, arrayBytes);
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

template <typename T> void Limiter<T>::cleanInternalBuffers() noexcept
{
   if (_gainState != nullptr) {
      (void) memset(_gainState, 0, _numChannels * sizeof(T));
   }
}

template <typename T> void Limiter<T>::bypassCopy() noexcept
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

template <typename T> void Limiter<T>::exec() noexcept
{
   if (_bypassMode != 0U) {
      bypassCopy();
   }
   else {
      T *pX = static_cast<T *>(_in.pBuf);
      T *pY = static_cast<T *>(_out.pBuf);

      TISP_limiter_abs_exec(_handle_abs.get(), pX, pY);
      AUDIOLIB_dB20_exec(_handle_dB20.get(), pY, _scratch);

      TISP_limiter_gainComputer_exec(_handle_gainComputer.get(), _scratch, pY);
      AUDIOLIB_undB20_exec(_handle_undB20.get(), pY, _scratch);

      TISP_limiter_mul_exec(_handle_mul.get(), pX, _scratch, pY);
   }
}

template <typename T>
int32_t Limiter<T>::TISP_limiter_abs_init(void                   *handle,
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
int32_t Limiter<T>::TISP_limiter_mul_init(void                   *handle,
                                          AUDIOLIB_bufParams2D_t *bufParamsIn0,
                                          AUDIOLIB_bufParams2D_t *bufParamsIn1,
                                          AUDIOLIB_bufParams2D_t *bufParamsOut)
{
   int32_t status = 0;
   // Setup SE/SA for abs operation
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

__INLINE_FUNC(TISP_limiter_abs_exec)
template <typename T> inline void Limiter<T>::TISP_limiter_abs_exec(void *handle, T *pIn, T *pOut)
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

      vec vOut = __abs(vIn);

      __vpred vPred = c7x::strm_agen<0, vec>::get_vpred();
      vec    *vAddr = c7x::strm_agen<0, vec>::get_adv(pOutLocal);
      __vstore_pred(vPred, vAddr, vOut);
   }

   // Close SE/SA
   __SE0_CLOSE();
   __SA0_CLOSE();

   return;
}

__INLINE_FUNC(TISP_limiter_mul_exec)
template <typename T> inline void Limiter<T>::TISP_limiter_mul_exec(void *handle, T *pIn0, T *pIn1, T *pOut)
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
int32_t Limiter<T>::TISP_limiter_gainComputer_init(void                   *handle,
                                                   AUDIOLIB_bufParams2D_t *bufParamsIn,
                                                   AUDIOLIB_bufParams1D_t *bufParamsGain,
                                                   AUDIOLIB_bufParams2D_t *bufParamsOut)
{
   int32_t          status    = 0;
   __SE_TEMPLATE_v1 se0Params = __gen_SE_TEMPLATE_v1();
   __SE_TEMPLATE_v1 se1Params = __gen_SE_TEMPLATE_v1();

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

   // Fetch 1D threshold and kneeWidth buffers with SE1
   se1Params.DIMFMT  = __SE_DIMFMT_3D;
   se1Params.VECLEN  = c7x::se_veclen<vec>::value;
   se1Params.ELETYPE = c7x::se_eletype<vec>::value;

   se1Params.ICNT0 = static_cast<uint32_t>(elementCount);
   se1Params.DIM1  = static_cast<int32_t>(this->_kneeWidthDb - this->_thresholdDb);
   se1Params.ICNT1 = static_cast<uint32_t>(2);
   se1Params.DIM2  = static_cast<int32_t>(elementCount);
   se1Params.ICNT2 = static_cast<uint32_t>(AUDIOLIB_ceilingDiv(bufParamsGain->dim_x, elementCount));

   se1Params.DECDIM1       = __SE_DECDIM_DIM2;
   se1Params.DECDIM1_WIDTH = static_cast<uint32_t>(bufParamsGain->dim_x);
   se1Params.DECDIM1SD     = __SE_DECDIMSD_DIM0;

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

   // Fetch 1D attack and release coeff buffers with SA2, SA3 (Use same SA parameters for address generation pattern)
   sa1Params.DIMFMT = __SA_DIMFMT_1D;
   sa1Params.VECLEN = c7x::sa_veclen<vec>::value;

   sa1Params.ICNT0 = static_cast<uint32_t>(bufParamsGain->dim_x);

   uint8_t *handle_gainComputer               = static_cast<uint8_t *>(handle);
   *(static_cast<__SE_TEMPLATE_v1 *>(handle)) = se0Params;
   *(static_cast<__SE_TEMPLATE_v1 *>(static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1)))) = se1Params;
   *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_gainComputer + 2 * sizeof(__SE_TEMPLATE_v1)))) =
       sa0Params;
   *(static_cast<__SA_TEMPLATE_v1 *>(
       static_cast<void *>(handle_gainComputer + 2 * sizeof(__SE_TEMPLATE_v1) + sizeof(__SA_TEMPLATE_v1)))) = sa1Params;

   return status;
}

__INLINE_FUNC(TISP_limiter_gainComputer_exec)
template <typename T> inline void Limiter<T>::TISP_limiter_gainComputer_exec(void *handle, T *pIn, T *pOut)
{
   // Extract SE/SA params
   uint8_t         *handle_gainComputer = static_cast<uint8_t *>(handle);
   __SE_TEMPLATE_v1 se0Params           = *(static_cast<__SE_TEMPLATE_v1 *>(handle));
   __SE_TEMPLATE_v1 se1Params =
       *(static_cast<__SE_TEMPLATE_v1 *>(static_cast<void *>(handle_gainComputer + sizeof(__SE_TEMPLATE_v1))));
   __SA_TEMPLATE_v1 sa0Params =
       *(static_cast<__SA_TEMPLATE_v1 *>(static_cast<void *>(handle_gainComputer + 2 * sizeof(__SE_TEMPLATE_v1))));
   __SA_TEMPLATE_v1 sa1Params = *(static_cast<__SA_TEMPLATE_v1 *>(
       static_cast<void *>(handle_gainComputer + 2 * sizeof(__SE_TEMPLATE_v1) + sizeof(__SA_TEMPLATE_v1))));

   // Assign local buffer addresses
   T *pThresholdLocal    = static_cast<T *>(this->_thresholdDb);
   T *pKneeWidthLocal    = static_cast<T *>(this->_kneeWidthDb);
   T *pAttackCoeffLocal  = static_cast<T *>(this->_attackCoeffs);
   T *pReleaseCoeffLocal = static_cast<T *>(this->_releaseCoeffs);
   T *pGainStateLocal    = static_cast<T *>(this->_gainState);

   // Update SE1 parameters with correct dimension jump between buffer addresses
   se1Params.DIM1 = static_cast<int32_t>(pKneeWidthLocal - pThresholdLocal);

   T *pInLocal  = static_cast<T *>(pIn);
   T *pOutLocal = static_cast<T *>(pOut);

   using vec             = typename c7x::make_full_vector<T>::type;
   uint32_t elementCount = c7x::element_count_of<vec>::value;

   uint32_t numBlocks = static_cast<uint32_t>(AUDIOLIB_ceilingDiv(this->_numChannels, elementCount));

   // Open SE/SA
   __SE0_OPEN(pInLocal, se0Params);
   __SE1_OPEN(pThresholdLocal, se1Params);
   __SA0_OPEN(sa0Params);
   __SA1_OPEN(sa1Params);
   __SA2_OPEN(sa1Params);
   __SA3_OPEN(sa1Params);

   // Define constant vectors
   vec vZero         = (vec) 0;   // NOLINT(MISRA.C_CAST)
   vec vOne          = (vec) 1;   // NOLINT(MISRA.C_CAST)
   vec vHalf         = (vec) 0.5; // NOLINT(MISRA.C_CAST)
   vec vSmoothedGain = vZero;
   // Iterate through channel blocks
#pragma COALESCE_LOOP
   for (uint32_t ch = 0; ch < numBlocks; ch++) {

      // Fetch per channel parameters
      vec vThreshold = c7x::strm_eng<1, vec>::get_adv(); // SE1
      vec vKneeWidth = c7x::strm_eng<1, vec>::get_adv(); // SE1

      __vpred vPredAttack  = c7x::strm_agen<2, vec>::get_vpred();
      vec    *vAddrAttack  = c7x::strm_agen<2, vec>::get_adv(pAttackCoeffLocal);
      vec     vAttackCoeff = __vload_pred(vPredAttack, vAddrAttack); // D

      __vpred vPredRelease  = c7x::strm_agen<3, vec>::get_vpred();
      vec    *vAddrRelease  = c7x::strm_agen<3, vec>::get_adv(pReleaseCoeffLocal);
      vec     vReleaseCoeff = __vload_pred(vPredRelease, vAddrRelease); // D

      vec vOneMinusAttackCoeff  = vOne - vAttackCoeff;  // L, S, C
      vec vOneMinusReleaseCoeff = vOne - vReleaseCoeff; // L, S, C

      // Soft Knee Regions
      vec vKneeWidthHalfBand      = vKneeWidth * vHalf;      // M, N
      vec vKneeWidthDoubleBand    = vKneeWidth + vKneeWidth; // L, S, C
      vec vInvKneeWidthDoubleBand = Limiter<T>::TISP_limiter_div_exec<vec>(vKneeWidthDoubleBand);
      vec vSoftKneeStart          = vThreshold - vKneeWidthHalfBand; // L, S, C
      vec vSoftKneeEnd            = vThreshold + vKneeWidthHalfBand; // L, S, C

      __vpred cmpHardKneePolicy = __cmp_le_pred(vKneeWidth, vZero);

      // Automatic make-up gain is a per-channel CONSTANT: M = -x_sc(0 dB), the static
      // characteristic evaluated at full scale (MATLAB `limiter`, MakeUpGainMode = 'Auto').
      // It depends on the threshold and the knee width alone, never on the instantaneous
      // sample level, so it is computed once here rather than per sample. The three
      // branches mirror the static characteristic in the sample loop below with vIn
      // replaced by zero.
      __vpred cmpHardKneeFullScale     = __cmp_ge_pred(vZero, vThreshold);     // 0 >= T
      __vpred cmpSoftKneeFullScale     = __cmp_gt_pred(vZero, vSoftKneeEnd);   // 0 >  knee end
      __vpred cmpSoftKneeNoneFullScale = __cmp_lt_pred(vZero, vSoftKneeStart); // 0 <  knee start

      vec vRatioFullScale  = vZero - vThreshold + vKneeWidthHalfBand;
      vec vRatio2FullScale = vRatioFullScale * vRatioFullScale;

      // NaN on hard-knee lanes (vInvKneeWidthDoubleBand is 1/0 there); discarded by the
      // cmpHardKneePolicy select below, exactly as in the sample loop.
      vec vSoftKneeFullScale = vZero - vRatio2FullScale * vInvKneeWidthDoubleBand;

      vec vHardKneeCharacteristicFullScale = __select(cmpHardKneeFullScale, vThreshold, vZero);

      vec vSoftKneeCharacteristicFullScale =
          __select(cmpSoftKneeFullScale, vThreshold, __select(cmpSoftKneeNoneFullScale, vZero, vSoftKneeFullScale));

      vec vStaticCharacteristicFullScale =
          __select(cmpHardKneePolicy, vHardKneeCharacteristicFullScale, vSoftKneeCharacteristicFullScale);

      vec vMakeUpGain = vZero - vStaticCharacteristicFullScale;

      // Resume this channel-block's envelope where the previous exec() left it.
      __vpred vPredState = c7x::strm_agen<1, vec>::get_vpred();
      vec    *vAddrState = c7x::strm_agen<1, vec>::get_adv(pGainStateLocal);
      vSmoothedGain      = __vload_pred(vPredState, vAddrState);

#pragma MUST_ITERATE(1, 65535, )
      for (size_t n = 0; n < this->_blockSize; n++) {

         // fetch input vector (in decibel)
         vec vIn = c7x::strm_eng<0, vec>::get_adv();

         // Determine knee region predicate vectors. The knee policy is applied once at the
         // join below, so these are not masked with it individually.
         __vpred cmpHardKneeThreshold = __cmp_ge_pred(vIn, vThreshold);
         __vpred cmpSoftKneeThreshold = __cmp_gt_pred(vIn, vSoftKneeEnd);
         __vpred cmpSoftKneeNone      = __cmp_lt_pred(vIn, vSoftKneeStart);

         // Calculate soft knee region vector
         vec vRatio  = vIn - vThreshold + vKneeWidthHalfBand;
         vec vRatio2 = vRatio * vRatio;

         vec vSoftKnee = vIn - vRatio2 * vInvKneeWidthDoubleBand;

         // Hard knee: clamp to threshold at or above it, pass through below it
         vec vHardKneeCharacteristic = __select(cmpHardKneeThreshold, vThreshold, vIn);

         // Soft knee: threshold above the knee end, pass through below the knee start,
         // interpolated soft-knee value in between
         vec vSoftKneeCharacteristic =
             __select(cmpSoftKneeThreshold, vThreshold, __select(cmpSoftKneeNone, vIn, vSoftKnee));

         // Select the per-lane knee policy
         vec vStaticCharacteristic =
             __select(cmpHardKneePolicy, vHardKneeCharacteristic, vSoftKneeCharacteristic);

         // Compute target gain
         vec vTargetGain = vStaticCharacteristic - vIn;

         // Determine smoothing coefficients
         __vpred cmpSmoothingMode = __cmp_le_pred(vTargetGain, vSmoothedGain);

         // Compute smoothed gains with both mode coefficients
         vec vSmoothedGainRelease = vReleaseCoeff * vSmoothedGain + vOneMinusReleaseCoeff * vTargetGain;
         vec vSmoothedGainAttack  = vAttackCoeff * vSmoothedGain + vOneMinusAttackCoeff * vTargetGain;

         vSmoothedGain = __select(cmpSmoothingMode, vSmoothedGainAttack, vSmoothedGainRelease);

         // Apply the constant automatic make-up gain
         vec vGain = vSmoothedGain + vMakeUpGain;

         // Store smoothed gains
         __vpred vPredOut = c7x::strm_agen<0, vec>::get_vpred();
         vec    *vAddrOut = c7x::strm_agen<0, vec>::get_adv(pOutLocal);
         __vstore_pred(vPredOut, vAddrOut, vGain);
      }

      // Carry this block's final envelope to the next exec().
      __vstore_pred(vPredState, vAddrState, vSmoothedGain);
   }

   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();
   __SA1_CLOSE();
   __SA2_CLOSE();
   __SA3_CLOSE();

   return;
}

__INLINE_FUNC(TISP_limiter_div_exec)
template <typename T> template <typename vecType> inline vecType Limiter<T>::TISP_limiter_div_exec(vecType vIn)
{

   vecType vTwo     = (vecType) 2; // NOLINT(MISRA.C_CAST)
   vecType vRecipIn = __recip(vIn);
   vecType vIter1   = vRecipIn * vIn;
   vecType vDiff    = vTwo - vIter1;
   vecType vInverse = vRecipIn * vDiff;

   return vInverse;
}

template class Limiter<float>;

} // namespace AUDIOLIB
} // namespace TISP
