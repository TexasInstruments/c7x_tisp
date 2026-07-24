// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "c7x_scalable.h"
#include "common/DSPLIB_types.h"
#include <TISP_fir.hpp>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [DSPLIB_fir constructor]

template <typename T>
Fir<T>::Fir(T         *pX,
            T         *pS,
            T         *pH,
            T         *pY,
            size_t     inputSize,
            size_t     filterSize,
            size_t     stateSize,
            size_t     batchSize,
            size_t     pitchX,
            size_t     pitchS,
            size_t     pitchH,
            size_t     pitchY,
            bool       enableNchCoefs,
            bool       enableMMA,
            bool       enableQ,
            size_t     Q,
            ErrorCtxt &errorCtx)

    : _pX(pX), _pS(pS), _pH(pH), _pY(pY), _inputSize(inputSize), _filterSize(filterSize), _batchSize(batchSize),
      _stateSize(stateSize), _pitchX(pitchX), _pitchS(pitchS), _pitchH(pitchH), _pitchY(pitchY),
      _enableNchCoefs(enableNchCoefs), _enableMMA(enableMMA), _enableQ(enableQ), _Q(Q), _bypassMode(0U)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "inputSize=%zu,filterSize=%zu,batchSize=%zu",
                   inputSize, filterSize, batchSize);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Fir constructor", "input pointer pX is null");
   }
   else if (pS == nullptr) {
      errorCtx.setNullPointerError("Fir constructor", "state pointer pS is null");
   }
   else if (pH == nullptr) {
      errorCtx.setNullPointerError("Fir constructor", "filter pointer pH is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("Fir constructor", "output pointer pY is null");
   }
   else if (inputSize == 0) {
      errorCtx.setInvalidParameterError("Fir constructor", "inputSize must be greater than zero");
   }
   else if (filterSize == 0) {
      errorCtx.setInvalidParameterError("Fir constructor", "filterSize must be greater than zero");
   }
   else if (stateSize == 0) {
      errorCtx.setInvalidParameterError("Fir constructor", "stateSize must be greater than zero");
   }
   else if (batchSize == 0) {
      errorCtx.setInvalidParameterError("Fir constructor", "batchSize must be greater than zero");
   }
   else if (static_cast<int32_t>(Q) < 0) {
      errorCtx.setInvalidParameterError("Fir constructor", "Q must be greater than zero");
   }
   else {
      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_fir from DSPLIB */
      /***********************************************************************/

      // alias for vector type
      using vec = typename c7x::make_full_vector<T>::type;

      // populate init args from constructor
      DSPLIB_fir_InitArgs kerInitArgs;
      kerInitArgs.dataSize       = stateSize;
      kerInitArgs.batchSize      = batchSize;
      kerInitArgs.filterSize     = filterSize;
      kerInitArgs.enableNchCoefs = enableNchCoefs;

      kerInitArgs.enableMMA = enableMMA;
      kerInitArgs.MMA_SIZE  = c7x::element_count_of<vec>::value;
      kerInitArgs.enableQ   = enableQ;
      kerInitArgs.Q         = Q;

      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_fir_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Fir constructor", "failed to allocate fir handle");
      }

      if (errorCtx.isSuccess()) {

         // instantiate appropriate bufparams for all buffers
         DSPLIB_bufParams2D_t bufParamsFilter;
         DSPLIB_bufParams2D_t bufParamsState;
         DSPLIB_bufParams2D_t bufParamsOut;

         bufParamsState.data_type = DSPLIB_element_dType<T>::value;
         bufParamsState.dim_x     = static_cast<uint32_t>(stateSize);
         bufParamsState.dim_y     = static_cast<uint32_t>(batchSize);
         bufParamsState.stride_y  = static_cast<int32_t>(pitchS);

         bufParamsFilter.data_type = DSPLIB_element_dType<T>::value;
         bufParamsFilter.dim_x     = static_cast<uint32_t>(filterSize);
         bufParamsFilter.dim_y     = static_cast<uint32_t>(batchSize);
         bufParamsFilter.stride_y  = static_cast<int32_t>(pitchH);

         bufParamsOut.data_type = DSPLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(inputSize);
         bufParamsOut.dim_y     = static_cast<uint32_t>(batchSize);
         bufParamsOut.stride_y  = static_cast<int32_t>(pitchY);

         // Initialize bufParams with node input and output buffer parameters
         _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
         _nodeInfo.bufParams.isInputInterleave = 0;
         _nodeInfo.bufParams.widthIn[0]        = inputSize;
         _nodeInfo.bufParams.heightIn[0]       = batchSize;
         _nodeInfo.bufParams.inPitchBytes[0]   = pitchX;

         _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
         _nodeInfo.bufParams.isOutputInterleave = 0;
         _nodeInfo.bufParams.widthOut[0]        = inputSize;
         _nodeInfo.bufParams.heightOut[0]       = batchSize;
         _nodeInfo.bufParams.outPitchBytes[0]   = pitchY;

         /******************************************************************************/
         /* Call init function from DSPLIB and set SE/SA params */
         /******************************************************************************/

         //  buffer parameters custom to FIR kernel

         DSPLIB_STATUS status = DSPLIB_fir_getSizes(&kerInitArgs, &bufParamsState, &bufParamsFilter, &bufParamsOut);

         errorCtx.remapXLIBError(status, "DSPLIB_fir_getSizes");

         if (errorCtx.isSuccess()) {
            status = DSPLIB_fir_init_checkParams(_handle.get(), &bufParamsState, &bufParamsFilter, &bufParamsOut,
                                                 &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_fir_init_checkParams");

            if (errorCtx.isSuccess()) {
               status =

                   DSPLIB_fir_init(_handle.get(), &bufParamsState, &bufParamsFilter, &bufParamsOut, &kerInitArgs);
               errorCtx.remapXLIBError(status, "DSPLIB_fir_init");

               if (errorCtx.isSuccess()) {

                  // initialize SE/SA params for block copy transactions
                  _se0ParamsPre.ELETYPE = c7x::se_eletype<vec>::value;
                  _se0ParamsPre.VECLEN  = c7x::se_veclen<vec>::value;
                  _se0ParamsPre.DIMFMT  = __SE_DIMFMT_2D;

                  _se0ParamsPre.ICNT0 = static_cast<uint32_t>(inputSize);
                  _se0ParamsPre.DIM1  = static_cast<int32_t>(pitchX / sizeof(T));
                  _se0ParamsPre.ICNT1 = static_cast<uint32_t>(batchSize);

                  _sa0ParamsPre.VECLEN = c7x::sa_veclen<vec>::value;
                  _sa0ParamsPre.DIMFMT = __SA_DIMFMT_2D;

                  _sa0ParamsPre.ICNT0 = static_cast<uint32_t>(inputSize);
                  _sa0ParamsPre.DIM1  = static_cast<int32_t>(pitchS / sizeof(T));
                  _sa0ParamsPre.ICNT1 = static_cast<uint32_t>(batchSize);

                  _se0ParamsPost.ELETYPE = c7x::se_eletype<vec>::value;
                  _se0ParamsPost.VECLEN  = c7x::se_veclen<vec>::value;
                  _se0ParamsPost.DIMFMT  = __SE_DIMFMT_2D;

                  _se0ParamsPost.ICNT0 = static_cast<uint32_t>(filterSize - 1U);
                  _se0ParamsPost.DIM1  = static_cast<int32_t>(pitchS / sizeof(T));
                  _se0ParamsPost.ICNT1 = static_cast<uint32_t>(batchSize);

                  _sa0ParamsPost.VECLEN = c7x::sa_veclen<vec>::value;
                  _sa0ParamsPost.DIMFMT = __SA_DIMFMT_2D;

                  _sa0ParamsPost.ICNT0 = static_cast<uint32_t>(filterSize - 1U);
                  _sa0ParamsPost.DIM1  = static_cast<int32_t>(pitchS / sizeof(T));
                  _sa0ParamsPost.ICNT1 = static_cast<uint32_t>(batchSize);

                  cleanInternalBuffers();
               }
            }
         }
      }
   }
}

//! [DSPLIB_fir constructor]

template <typename T> void *Fir<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Fir<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Fir<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Fir<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Fir<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<T *>(addr.src[0]);
   }
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<T *>(addr.src[0]);
      _pS = static_cast<T *>(addr.src[1]);
      _pH = static_cast<T *>(addr.src[2]);
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void Fir<T>::bypassCopy() noexcept
{
   const size_t rowBytes = _inputSize * sizeof(T);

   if (_pX != _pY) {
      if ((_pitchX == rowBytes) && (_pitchY == rowBytes)) {
         (void) std::memcpy(_pY, _pX, rowBytes * _batchSize);
      }
      else {
         uint8_t       *pDst = reinterpret_cast<uint8_t *>(_pY);
         const uint8_t *pSrc = reinterpret_cast<const uint8_t *>(_pX);

         for (size_t i = 0; i < _batchSize; i++) {
            (void) std::memcpy(&pDst[i * _pitchY], &pSrc[i * _pitchX], rowBytes); // NOLINT(MISRA.PTR.ARITH)
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
      // copy from input to state
      TISP_fir_blockCopy2D(_pX, &_pS[_filterSize - 1U], _inputSize, _batchSize, _se0ParamsPre,
                           _sa0ParamsPre); // NOLINT(MISRA.PTR.ARITH)

      // execute fir kernel
      DSPLIB_fir_exec(_handle.get(), _pS, _pH, _pY);

      // add delay lines to state buffer
      TISP_fir_blockCopy2D(&_pS[_inputSize], _pS, (_filterSize - 1U), _batchSize, _se0ParamsPost,
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

template <typename T>
Fir<T>::Fir(T          *pX,
            T          *pS,
            T          *pH,
            T          *pY,
            size_t      inputSize,
            size_t      filterSize,
            size_t      stateSize,
            size_t      batchSize,
            size_t      pitchX,
            size_t      pitchS,
            size_t      pitchH,
            size_t      pitchY,
            bool        enableNchCoefs,
            bool        enableMMA,
            bool        enableQ,
            size_t      Q,
            const char *nodeName,
            uint32_t    nodeId,
            ErrorCtxt  &errorCtx)
    : Fir(pX,
          pS,
          pH,
          pY,
          inputSize,
          filterSize,
          stateSize,
          batchSize,
          pitchX,
          pitchS,
          pitchH,
          pitchY,
          enableNchCoefs,
          enableMMA,
          enableQ,
          Q,
          errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> bool Fir<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Fir::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<FirParamId>(paramId)) {
      case FirParamId::kFilterCoeffs: {
         size_t numChannels  = _enableNchCoefs ? _batchSize : 1U;
         size_t expectedSize = _filterSize * numChannels * sizeof(T);

         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("Fir::setParams", "Value size smaller than expected for kFilterCoeffs");
         }
         else {
            cleanInternalBuffers();
            const uint8_t *coeffData = static_cast<const uint8_t *>(value);
            for (size_t c = 0; c < numChannels; c++) {
               uint8_t *dst = reinterpret_cast<uint8_t *>(_pH) + (c * _pitchH);
               (void) std::memcpy(dst, coeffData + (c * _filterSize * sizeof(T)), _filterSize * sizeof(T));
            }
         }
         break;
      }
      case FirParamId::kBypass: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Fir::setParams", "Value size smaller than expected for kBypass");
         }
         else {
            uint32_t bypassMode = 0U;
            (void) std::memcpy(&bypassMode, value, sizeof(uint32_t));
            if (bypassMode > 1U) {
               errorCtx.setInvalidParameterError("Fir::setParams", "bypass mode must be either zero or one");
            }
            else {
               /* Leaving bypass: the delay line in _pS still holds the tail of the last block
                * processed before bypass was engaged, so convolving fresh input against it would
                * splice two unrelated signals. Discard it and resume from a cold start. */
               if ((_bypassMode != 0U) && (bypassMode == 0U)) {
                  cleanInternalBuffers();
               }
               _bypassMode = bypassMode;
            }
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
   if (value == nullptr) {
      errorCtx.setNullPointerError("Fir::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("Fir::getParams", "size pointer is null");
   }
   else {
      size_t numChannels = _enableNchCoefs ? _batchSize : 1U;

      switch (static_cast<FirParamId>(paramId)) {
      case FirParamId::kFilterCoeffs: {
         size_t expectedSize = _filterSize * numChannels * sizeof(T);
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("Fir::getParams", "Provided buffer size is too small for kFilterCoeffs");
         }
         else {
            uint8_t *coeffData = static_cast<uint8_t *>(value);
            for (size_t c = 0; c < numChannels; c++) {
               const uint8_t *src = reinterpret_cast<const uint8_t *>(_pH) + (c * _pitchH);
               (void) std::memcpy(coeffData + (c * _filterSize * sizeof(T)), src, _filterSize * sizeof(T));
            }
         }
         *size = expectedSize;
         break;
      }
      case FirParamId::kBypass: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Fir::getParams", "Provided buffer size is too small for kBypass");
         }
         else {
            (void) std::memcpy(value, &_bypassMode, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
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
   if (_pS != nullptr && _batchSize > 0 && _stateSize > 0) {
      /* Only the first _stateSize elements of each row are live: the kernel indexes at most
       * outputSize + filterSize - 2 == _stateSize - 1 within a row, and the pre/post block copies in
       * exec() stay inside that same range. The rest of the row up to _pitchS is padding the kernel
       * never touches. Clear row by row and leave the padding alone. */
      for (size_t row = 0; row < _batchSize; row++) {
         uint8_t *pRow = reinterpret_cast<uint8_t *>(_pS) + (row * _pitchS); // NOLINT(MISRA.PTR.ARITH)
         std::memset(pRow, 0, _stateSize * sizeof(T));
      }
   }
}

template class Fir<float>;

} // namespace DSPLIB
} // namespace TISP
