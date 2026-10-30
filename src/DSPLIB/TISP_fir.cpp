// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "c7x_scalable.h"
#include "common/DSPLIB_types.h"
#include <TISP_fir.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [DSPLIB_fir constructor]

template <typename T>
Fir<T>::Fir(const TISP_BufDescList    *pInputs,
            const TISP_BufDescList    *pOutputs,
            const TISP_BufDescList    *pStates,
            const FirConfig      *pConfig,
            const FirTunableParams<T> *pDefaultTunableParams,
            const char                *nodeName,
            uint32_t                   nodeId,
            ErrorCtxt                 &errorCtx)
    : _in{}, _out{}, _state{}, _filterSize(0U), _numCoeffRows(0U), _coeffPitchBytes(0U), _coeffOffset(0U),
      _coeffs(nullptr), _bypassMode(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("Fir constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("Fir constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pCoeffs == nullptr) {
      errorCtx.setNullPointerError("Fir constructor", "coefficient pointer pDefaultTunableParams->pCoeffs is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output, and one state buffer
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Fir constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Fir constructor (outputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pStates, 1U, 1U, "Fir constructor (states)", errorCtx)) {
      _in    = pInputs->pDesc[0];
      _out   = pOutputs->pDesc[0];
      _state = pStates->pDesc[0];

      if ((_in.isInterleave != 0U) || (_out.isInterleave != 0U) || (_state.isInterleave != 0U)) {
         errorCtx.setInvalidParameterError("Fir constructor", "input, output, and state must be non-interleaved");
      }
      else if (pConfig->filterSize == 0U) {
         errorCtx.setInvalidParameterError("Fir constructor", "filterSize must be greater than zero");
      }
      else if ((_out.width != _in.width) || (_out.height != _in.height)) {
         errorCtx.setInvalidParameterError("Fir constructor", "output width and height must match the input");
      }
      else if ((_state.width != (_in.width + pConfig->filterSize - 1U)) || (_state.height != _in.height)) {
         errorCtx.setInvalidParameterError("Fir constructor",
                                           "state must have the input height and input width + filterSize - 1");
      }
      else if (pDefaultTunableParams->bypassMode > 1U) {
         errorCtx.setInvalidParameterError("Fir constructor", "bypass mode must be either zero or one");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      using vec = typename c7x::make_full_vector<T>::type;

      const size_t mmaSize = static_cast<size_t>(c7x::element_count_of<vec>::value);

      _filterSize      = pConfig->filterSize;
      _numCoeffRows    = pConfig->enableNchCoefs ? _in.height : 1U;
      _coeffPitchBytes = pConfig->coeffPitchBytes;
      _coeffOffset     = pConfig->enableMMA ? (2U * mmaSize) : 0U;
      _coeffs          = pDefaultTunableParams->pCoeffs;
      _bypassMode      = pDefaultTunableParams->bypassMode;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "inputSize=%zu,filterSize=%zu,batchSize=%zu",
                      _in.width, _filterSize, _in.height);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_fir from DSPLIB       */
      /***********************************************************************/

      DSPLIB_fir_InitArgs kerInitArgs{};
      kerInitArgs.funcStyle      = DSPLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.dataSize       = static_cast<int32_t>(_state.width);
      kerInitArgs.batchSize      = static_cast<int32_t>(_in.height);
      kerInitArgs.filterSize     = static_cast<int32_t>(_filterSize);
      kerInitArgs.enableNchCoefs = pConfig->enableNchCoefs ? 1U : 0U;
      kerInitArgs.enableMMA      = pConfig->enableMMA ? 1U : 0U;
      kerInitArgs.MMA_SIZE       = static_cast<int32_t>(mmaSize);
      kerInitArgs.enableQ        = pConfig->enableQ ? 1U : 0U;
      kerInitArgs.Q              = static_cast<uint32_t>(pConfig->Q);

      _handle = make_unique_handle<char>(DSPLIB_fir_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Fir constructor", "failed to allocate fir handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsState = TISP_toDsplibBufParams2D<T>(_state);
         DSPLIB_bufParams2D_t bufParamsOut   = TISP_toDsplibBufParams2D<T>(_out);
         DSPLIB_bufParams2D_t bufParamsFilter;

         bufParamsFilter.data_type = static_cast<uint32_t>(DSPLIB_element_dType<T>::value);
         bufParamsFilter.dim_x     = static_cast<uint32_t>(_filterSize);
         bufParamsFilter.dim_y     = static_cast<uint32_t>(_numCoeffRows);
         bufParamsFilter.stride_y  = static_cast<int32_t>(_coeffPitchBytes);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         DSPLIB_STATUS status = DSPLIB_fir_getSizes(&kerInitArgs, &bufParamsState, &bufParamsFilter, &bufParamsOut);

         if (errorCtx.remapXLIBError(status, "DSPLIB_fir_getSizes")) {
            // getSizes replaces the coefficient pitch with the padded MMA layout when MMA is enabled
            if (static_cast<size_t>(bufParamsFilter.stride_y) != _coeffPitchBytes) {
               errorCtx.setInvalidParameterError("Fir constructor",
                                                 "coeffPitchBytes does not match the pitch required by DSPLIB_fir");
            }
            else {
               status = DSPLIB_fir_init_checkParams(_handle.get(), &bufParamsState, &bufParamsFilter, &bufParamsOut,
                                                    &kerInitArgs);
            }
         }

         /******************************************************************************/
         /* Call init function from DSPLIB and set SE/SA params                        */
         /******************************************************************************/

         if (errorCtx.isSuccess() && errorCtx.remapXLIBError(status, "DSPLIB_fir_init_checkParams")) {
            status = DSPLIB_fir_init(_handle.get(), &bufParamsState, &bufParamsFilter, &bufParamsOut, &kerInitArgs);

            if (errorCtx.remapXLIBError(status, "DSPLIB_fir_init")) {
               // input to state: copy the block behind the retained filter history
               _se0ParamsPre.ELETYPE = c7x::se_eletype<vec>::value;
               _se0ParamsPre.VECLEN  = c7x::se_veclen<vec>::value;
               _se0ParamsPre.DIMFMT  = __SE_DIMFMT_2D;
               _se0ParamsPre.ICNT0   = static_cast<uint32_t>(_in.width);
               _se0ParamsPre.DIM1    = static_cast<int32_t>(_in.pitchBytes / sizeof(T));
               _se0ParamsPre.ICNT1   = static_cast<uint32_t>(_in.height);

               _sa0ParamsPre.VECLEN = c7x::sa_veclen<vec>::value;
               _sa0ParamsPre.DIMFMT = __SA_DIMFMT_2D;
               _sa0ParamsPre.ICNT0  = static_cast<uint32_t>(_in.width);
               _sa0ParamsPre.DIM1   = static_cast<int32_t>(_state.pitchBytes / sizeof(T));
               _sa0ParamsPre.ICNT1  = static_cast<uint32_t>(_in.height);

               // state delay line: move the last filterSize - 1 samples to the front
               _se0ParamsPost.ELETYPE = c7x::se_eletype<vec>::value;
               _se0ParamsPost.VECLEN  = c7x::se_veclen<vec>::value;
               _se0ParamsPost.DIMFMT  = __SE_DIMFMT_2D;
               _se0ParamsPost.ICNT0   = static_cast<uint32_t>(_filterSize - 1U);
               _se0ParamsPost.DIM1    = static_cast<int32_t>(_state.pitchBytes / sizeof(T));
               _se0ParamsPost.ICNT1   = static_cast<uint32_t>(_in.height);

               _sa0ParamsPost.VECLEN = c7x::sa_veclen<vec>::value;
               _sa0ParamsPost.DIMFMT = __SA_DIMFMT_2D;
               _sa0ParamsPost.ICNT0  = static_cast<uint32_t>(_filterSize - 1U);
               _sa0ParamsPost.DIM1   = static_cast<int32_t>(_state.pitchBytes / sizeof(T));
               _sa0ParamsPost.ICNT1  = static_cast<uint32_t>(_in.height);

               cleanInternalBuffers();
            }
         }
      }
   }
}

//! [DSPLIB_fir constructor]

template <typename T> void *Fir<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *Fir<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Fir<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Fir<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Fir<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addr.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void Fir<T>::bypassCopy() noexcept
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

template <typename T> void Fir<T>::exec() noexcept
{
   if (_bypassMode != 0U) {
      bypassCopy();
   }
   else {
      T *pState = static_cast<T *>(_state.pBuf);

      // copy from input to state
      TISP_fir_blockCopy2D(static_cast<T *>(_in.pBuf), &pState[_filterSize - 1U], _in.width, _in.height,
                           _se0ParamsPre, _sa0ParamsPre); // NOLINT(MISRA.PTR.ARITH)

      // execute fir kernel
      DSPLIB_fir_exec(_handle.get(), _state.pBuf, _coeffs, _out.pBuf);

      // add delay lines to state buffer
      TISP_fir_blockCopy2D(&pState[_in.width], pState, (_filterSize - 1U), _in.height, _se0ParamsPost,
                           _sa0ParamsPost); // NOLINT(MISRA.PTR.ARITH)
   }
}

template <typename T>
void Fir<T>::TISP_fir_blockCopy2D(T               *pX,
                                  T               *pY,
                                  size_t           width,
                                  size_t           height,
                                  __SE_TEMPLATE_v1 se0Params,
                                  __SA_TEMPLATE_v1 sa0Params)
{

   using vec = typename c7x::make_full_vector<T>::type;

   // open SE0 to fetch Input samples
   __SE0_OPEN(pX, se0Params);

   // open SA0 to write output samples
   __SA0_OPEN(sa0Params);

   size_t numBlocks = (width + static_cast<size_t>(c7x::max_simd<T>::value) - 1U) /
                      static_cast<size_t>(c7x::max_simd<T>::value); // ceiling division

   for (size_t i = 0; i < height; i++) {
      for (size_t j = 0; j < numBlocks; j++) {
         __vpred tmp  = c7x::strm_agen<0, vec>::get_vpred();
         vec    *addr = c7x::strm_agen<0, vec>::get_adv(pY);
         __vstore_pred(tmp, addr, c7x::strm_eng<0, vec>::get_adv());
      }
   }

   // close SE/SA
   __SE0_CLOSE();
   __SA0_CLOSE();
}

template <typename T> void Fir<T>::writePackedCoeffs(const T *pPacked) noexcept
{
   uint8_t *pCoeffBytes = reinterpret_cast<uint8_t *>(_coeffs);

   for (size_t r = 0; r < _numCoeffRows; r++) {
      T *pRow = reinterpret_cast<T *>(&pCoeffBytes[r * _coeffPitchBytes]); // NOLINT(MISRA.PTR.ARITH)
      (void) memcpy(&pRow[_coeffOffset], &pPacked[r * _filterSize], _filterSize * sizeof(T)); // NOLINT(MISRA.PTR.ARITH)
   }
}

template <typename T> void Fir<T>::readPackedCoeffs(T *pPacked) const noexcept
{
   const uint8_t *pCoeffBytes = reinterpret_cast<const uint8_t *>(_coeffs);

   for (size_t r = 0; r < _numCoeffRows; r++) {
      const T *pRow = reinterpret_cast<const T *>(&pCoeffBytes[r * _coeffPitchBytes]); // NOLINT(MISRA.PTR.ARITH)
      (void) memcpy(&pPacked[r * _filterSize], &pRow[_coeffOffset], _filterSize * sizeof(T)); // NOLINT(MISRA.PTR.ARITH)
   }
}

template <typename T> bool Fir<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Fir::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<FirParamId>(paramId)) {
      case FirParamId::kFilterCoeffs: {
         if (size < (_numCoeffRows * _filterSize * sizeof(T))) {
            errorCtx.setInvalidParameterError("Fir::setParams", "Value size smaller than expected for kFilterCoeffs");
         }
         else {
            cleanInternalBuffers();
            writePackedCoeffs(static_cast<const T *>(value));
         }
         break;
      }
      case FirParamId::kBypass: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Fir::setParams", "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode = 0U;
            (void) memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("Fir::setParams", "bypass mode must be either zero or one");
            }
            else {
               // Leaving bypass: the delay line is stale, so resume from a cold start
               if ((_bypassMode != 0U) && (bypassMode == 0U)) {
                  cleanInternalBuffers();
               }
               _bypassMode = bypassMode;
            }
         }
         break;
      }
      case FirParamId::kAll: {
         const FirTunableParams<T> *pParams = static_cast<const FirTunableParams<T> *>(value);
         if (size < sizeof(FirTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Fir::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pCoeffs == nullptr) {
            errorCtx.setNullPointerError("Fir::setParams", "coefficient pointer pCoeffs is null");
         }
         else if (pParams->bypassMode > 1U) {
            errorCtx.setInvalidParameterError("Fir::setParams", "bypass mode must be either zero or one");
         }
         else {
            if (pParams->pCoeffs != _coeffs) {
               (void) memcpy(_coeffs, pParams->pCoeffs, _numCoeffRows * _coeffPitchBytes);
            }
            cleanInternalBuffers();
            _bypassMode = pParams->bypassMode;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Fir::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> bool Fir<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (size == nullptr) {
      errorCtx.setNullPointerError("Fir::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<FirParamId>(paramId)) {
      case FirParamId::kFilterCoeffs: {
         size_t expectedSize = _numCoeffRows * _filterSize * sizeof(T);
         if (value == nullptr) {
            errorCtx.setNullPointerError("Fir::getParams", "value pointer is null");
         }
         else if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("Fir::getParams", "Provided buffer size is too small for kFilterCoeffs");
         }
         else {
            readPackedCoeffs(static_cast<T *>(value));
         }
         *size = expectedSize;
         break;
      }
      case FirParamId::kBypass: {
         if (value == nullptr) {
            errorCtx.setNullPointerError("Fir::getParams", "value pointer is null");
         }
         else if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Fir::getParams", "Provided buffer size is too small for kBypass");
         }
         else {
            (void) memcpy(value, &_bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      case FirParamId::kAll: {
         FirTunableParams<T> *pParams = static_cast<FirTunableParams<T> *>(value);
         if (pParams == nullptr) {
            errorCtx.setNullPointerError("Fir::getParams", "value pointer is null");
         }
         else if (*size < sizeof(FirTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Fir::getParams", "Provided buffer size is too small for kAll");
         }
         else if (pParams->pCoeffs == nullptr) {
            errorCtx.setNullPointerError("Fir::getParams", "coefficient pointer pCoeffs is null");
         }
         else {
            if (pParams->pCoeffs != _coeffs) {
               (void) memcpy(pParams->pCoeffs, _coeffs, _numCoeffRows * _coeffPitchBytes);
            }
            pParams->bypassMode = _bypassMode;
         }
         *size = sizeof(FirTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Fir::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> void Fir<T>::cleanInternalBuffers() noexcept
{
   if (_state.pBuf != nullptr) {
      uint8_t *pStateBytes = static_cast<uint8_t *>(_state.pBuf);

      for (size_t row = 0; row < _state.height; row++) {
         (void) memset(&pStateBytes[row * _state.pitchBytes], 0, _state.width * sizeof(T)); // NOLINT(MISRA.PTR.ARITH)
      }
   }
}

template class Fir<float>;

} // namespace DSPLIB
} // namespace TISP
