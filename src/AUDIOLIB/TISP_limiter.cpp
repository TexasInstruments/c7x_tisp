// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_limiter.hpp>
#include <cstdint>
#include <cstring>
#include <random>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Limiter constructor]

template <typename T>
Limiter<T>::Limiter(T         *pX,
                    T         *pG,
                    T         *pS,
                    T         *pY,
                    T         *pThreshold_dB,
                    T         *pKneeWidth_dB,
                    T         *pAttackCoeff,
                    T         *pReleaseCoeff,
                    size_t     numChannels,
                    size_t     blockSize,
                    size_t     pitchX,
                    size_t     pitchS,
                    size_t     pitchY,
                    bool       isInterleaved,
                    ErrorCtxt &errorCtx)
    : _pX(pX), _pG(pG), _pS(pS), _pY(pY), _pThreshold_dB(pThreshold_dB), _pKneeWidth_dB(pKneeWidth_dB),
      _pAttackCoeff(pAttackCoeff), _pReleaseCoeff(pReleaseCoeff), _numChannels(numChannels), _blockSize(blockSize),
      _pitchX(pitchX), _pitchS(pitchS), _pitchY(pitchY), _isInterleaved(isInterleaved)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numChannels=%zu, blockSize=%zu, isInterleaved=%d",
                   this->_numChannels, this->_blockSize, this->_isInterleaved);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "input pointer pX is null");
   }
   else if (pG == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "gain pointer pG is null");
   }
   else if (pS == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "scratch pointer pS is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "output pointer pY is null");
   }
   else if (pThreshold_dB == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "threshold pointer pThreshold_dB is null");
   }
   else if (pKneeWidth_dB == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "kneewidth pointer pKneeWidth_dB is null");
   }
   else if (pAttackCoeff == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "attack coefficient pointer pAttackCoeff is null");
   }
   else if (pReleaseCoeff == nullptr) {
      errorCtx.setNullPointerError("Limiter constructor", "release coefficient pointer pReleaseCoeff is null");
   }
   else if (numChannels == 0) {
      errorCtx.setInvalidParameterError("Limiter constructor", "numChannels must be greater than zero");
   }
   else if (blockSize == 0) {
      errorCtx.setInvalidParameterError("Limiter constructor", "blockSize must be greater than zero");
   }
   else if (isInterleaved != true && isInterleaved != false) {
      errorCtx.setInvalidParameterError("Limiter constructor", "interleave must be either true or false");
   }
   else {
      // Determine width and height of processing
      size_t width  = (this->_isInterleaved) ? this->_numChannels : this->_blockSize;
      size_t height = (this->_isInterleaved) ? this->_blockSize : this->_numChannels;

      // Initialize bufParams with node input and output buffer parameters. Output slot 0 is
      // the audio buffer pY; output slot 1 is the per-channel gain vector pG (1D, numChannels).
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleaved ? 1 : 0;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = pitchX;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleaved ? 1 : 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = pitchY;
      _nodeInfo.bufParams.widthOut[1]        = numChannels;
      _nodeInfo.bufParams.heightOut[1]       = 1;
      _nodeInfo.bufParams.outPitchBytes[1]   = numChannels * sizeof(T);

      // Define buffer param structures
      AUDIOLIB_bufParams2D_t bufParamsIn;
      AUDIOLIB_bufParams2D_t bufParamsOut;
      AUDIOLIB_bufParams2D_t bufParamsScratch;
      AUDIOLIB_bufParams1D_t bufParamsGain;

      bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
      bufParamsIn.dim_x     = static_cast<uint32_t>(width);
      bufParamsIn.dim_y     = static_cast<uint32_t>(height);
      bufParamsIn.stride_y  = static_cast<int32_t>(pitchX);

      bufParamsScratch.data_type = AUDIOLIB_element_dType<T>::value;
      bufParamsScratch.dim_x     = static_cast<uint32_t>(width);
      bufParamsScratch.dim_y     = static_cast<uint32_t>(height);
      bufParamsScratch.stride_y  = static_cast<int32_t>(pitchS);

      bufParamsGain.data_type = AUDIOLIB_element_dType<T>::value;
      bufParamsGain.dim_x     = static_cast<uint32_t>(numChannels);

      bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
      bufParamsOut.dim_x     = static_cast<uint32_t>(width);
      bufParamsOut.dim_y     = static_cast<uint32_t>(height);
      bufParamsOut.stride_y  = static_cast<int32_t>(pitchY);

      /******************************************************************************/
      /* Call init function from AUDIOLIB to for limiter to set SE/SA params         */
      /******************************************************************************/

      //  abs init
      uint32_t handleSize_abs = sizeof(__SE_TEMPLATE_v1) + sizeof(__SA_TEMPLATE_v1);
      _handle_abs             = make_unique_handle<char>(handleSize_abs);
      if (!_handle_abs) {
         errorCtx.setMemoryError("Limiter constructor", "failed to allocate abs handle");
      }
      if (errorCtx.isSuccess()) {
         int32_t status = TISP_limiter_abs_init(_handle_abs.get(), &bufParamsIn, &bufParamsOut);
         errorCtx.remapXLIBError(status, "TISP_limiter_abs_init");
         if (errorCtx.isSuccess()) {
            //  dB20 init
            AUDIOLIB_dB20_InitArgs kerInitArgs_dB20;
            kerInitArgs_dB20.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;
            _handle_dB20               = make_unique_handle<char>(AUDIOLIB_dB20_getHandleSize(&kerInitArgs_dB20));
            if (!_handle_dB20) {
               errorCtx.setMemoryError("Limiter constructor", "failed to allocate dB20 handle");
            }
            if (errorCtx.isSuccess()) {
               status = AUDIOLIB_dB20_init(_handle_dB20.get(), &bufParamsOut, &bufParamsScratch, &kerInitArgs_dB20);
               errorCtx.remapXLIBError(status, "AUDIOLIB_dB20_init");
               if (errorCtx.isSuccess()) {
                  //  gainCompute + gainSmoothing + gainMakeUp init
                  uint32_t handleSize_gainComputer = 2 * sizeof(__SE_TEMPLATE_v1) + 2 * sizeof(__SA_TEMPLATE_v1);
                  _handle_gainComputer             = make_unique_handle<char>(handleSize_gainComputer);
                  if (!_handle_gainComputer) {
                     errorCtx.setMemoryError("Limiter constructor", "failed to allocate gainComputer handle");
                  }
                  if (errorCtx.isSuccess()) {
                     status = TISP_limiter_gainComputer_init(_handle_gainComputer.get(), &bufParamsScratch,
                                                             &bufParamsGain, &bufParamsOut);

                     errorCtx.remapXLIBError(status, "TISP_limiter_gainComputer_init");
                     if (errorCtx.isSuccess()) {
                        //  undB20 init
                        AUDIOLIB_undB20_InitArgs kerInitArgs_undB20;
                        kerInitArgs_undB20.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;
                        _handle_undB20 = make_unique_handle<char>(AUDIOLIB_undB20_getHandleSize(&kerInitArgs_undB20));
                        if (!_handle_undB20) {
                           errorCtx.setMemoryError("Limiter constructor", "failed to allocate undB20 handle");
                        }
                        if (errorCtx.isSuccess()) {
                           status = AUDIOLIB_undB20_init(_handle_undB20.get(), &bufParamsOut, &bufParamsScratch,
                                                         &kerInitArgs_undB20);
                           errorCtx.remapXLIBError(status, "AUDIOLIB_undB20_init");
                           if (errorCtx.isSuccess()) {
                              //  multiply init
                              uint32_t handleSize_mul = handleSize_abs + sizeof(__SE_TEMPLATE_v1);
                              _handle_mul             = make_unique_handle<char>(handleSize_mul);
                              if (!_handle_mul) {
                                 errorCtx.setMemoryError("Limiter constructor", "failed to allocate mul handle");
                              }
                              if (errorCtx.isSuccess()) {
                                 status = TISP_limiter_mul_init(_handle_mul.get(), &bufParamsIn, &bufParamsScratch,
                                                                &bufParamsOut);
                                 errorCtx.remapXLIBError(status, "TISP_limiter_mul_init");
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }
}

template <typename T> void *Limiter<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Limiter<T>::getOutputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   switch (index) {
   case 0:
      result = static_cast<void *>(_pY);
      break;
   case 1:
      result = static_cast<void *>(_pG);
      break;
   default:
      result = nullptr;
      break;
   }
   return result;
}

template <typename T> size_t Limiter<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Limiter<T>::getNumOutputs() const noexcept { return 2; }

//! [TISP::Limiter setAddr]
template <typename T> void Limiter<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<T *>(addr.src[0]);
   }

   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
      _pG = static_cast<T *>(addr.dst[1]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<T *>(addr.src[0]);
      _pY = static_cast<T *>(addr.dst[0]);
      _pG = static_cast<T *>(addr.dst[1]);
   }
}

template <typename T>
bool Limiter<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Limiter::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<LimiterParamId>(paramId)) {
      case LimiterParamId::kThreshold: {
         size_t expectedSize = _numChannels * sizeof(T);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("Limiter::setParams", "Value size smaller than expected for kThreshold");
         }
         else {
            const T *vals = reinterpret_cast<const T *>(value);
            for (size_t i = 0; i < _numChannels; ++i) {
               if (vals[i] > static_cast<T>(0.0)) {
                  errorCtx.setInvalidParameterError("Limiter::setParams", "kThreshold must be <= 0.0");
                  break;
               }
            }
            if (errorCtx.isSuccess()) {
               memcpy(_pThreshold_dB, value, expectedSize);
            }
         }
         break;
      }
      case LimiterParamId::kKneeWidth: {
         size_t expectedSize = _numChannels * sizeof(T);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("Limiter::setParams", "Value size smaller than expected for kKneeWidth");
         }
         else {
            const T *vals = reinterpret_cast<const T *>(value);
            for (size_t i = 0; i < _numChannels; ++i) {
               if (vals[i] < static_cast<T>(0.0)) {
                  errorCtx.setInvalidParameterError("Limiter::setParams", "kKneeWidth must be >= 0.0");
                  break;
               }
            }
            if (errorCtx.isSuccess()) {
               memcpy(_pKneeWidth_dB, value, expectedSize);
            }
         }
         break;
      }
      case LimiterParamId::kAttack: {
         size_t expectedSize = _numChannels * sizeof(T);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("Limiter::setParams", "Value size smaller than expected for kAttack");
         }
         else {
            const T *vals = reinterpret_cast<const T *>(value);
            for (size_t i = 0; i < _numChannels; ++i) {
               if (vals[i] < static_cast<T>(0.0) || vals[i] > static_cast<T>(1.0)) {
                  errorCtx.setInvalidParameterError("Limiter::setParams",
                                                    "kAttack coefficients must be between 0.0 and 1.0");
                  break;
               }
            }
            if (errorCtx.isSuccess()) {
               memcpy(_pAttackCoeff, value, expectedSize);
            }
         }
         break;
      }
      case LimiterParamId::kRelease: {
         size_t expectedSize = _numChannels * sizeof(T);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("Limiter::setParams", "Value size smaller than expected for kRelease");
         }
         else {
            const T *vals = reinterpret_cast<const T *>(value);
            for (size_t i = 0; i < _numChannels; ++i) {
               if (vals[i] < static_cast<T>(0.0) || vals[i] > static_cast<T>(1.0)) {
                  errorCtx.setInvalidParameterError("Limiter::setParams",
                                                    "kRelease coefficients must be between 0.0 and 1.0");
                  break;
               }
            }
            if (errorCtx.isSuccess()) {
               memcpy(_pReleaseCoeff, value, expectedSize);
            }
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Limiter::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool Limiter<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Limiter::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("Limiter::getParams", "size pointer is null");
   }
   else {
      size_t expectedSize = _numChannels * sizeof(T);
      if (*size < expectedSize) {
         errorCtx.setInvalidParameterError("Limiter::getParams", "Value size smaller than expected");
         *size = expectedSize;
      }
      else {
         switch (static_cast<LimiterParamId>(paramId)) {
         case LimiterParamId::kThreshold:
            memcpy(value, _pThreshold_dB, expectedSize);
            *size = expectedSize;
            break;
         case LimiterParamId::kKneeWidth:
            memcpy(value, _pKneeWidth_dB, expectedSize);
            *size = expectedSize;
            break;
         case LimiterParamId::kAttack:
            memcpy(value, _pAttackCoeff, expectedSize);
            *size = expectedSize;
            break;
         case LimiterParamId::kRelease:
            memcpy(value, _pReleaseCoeff, expectedSize);
            *size = expectedSize;
            break;
         default:
            errorCtx.setInvalidParameterError("Limiter::getParams", "Invalid paramId");
            break;
         }
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> void Limiter<T>::exec() noexcept
{

   TISP_limiter_abs_exec(_handle_abs.get(), _pX, _pY);
   AUDIOLIB_dB20_exec(_handle_dB20.get(), _pY, _pS);

   TISP_limiter_gainComputer_exec(_handle_gainComputer.get(), _pS, _pY, _pG);
   AUDIOLIB_undB20_exec(_handle_undB20.get(), _pY, _pS);

   TISP_limiter_mul_exec(_handle_mul.get(), _pX, _pS, _pY);
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

   size_t width  = (this->_isInterleaved) ? this->_numChannels : this->_blockSize;
   size_t height = (this->_isInterleaved) ? this->_blockSize : this->_numChannels;

   // flatten 2D structure into 1D
   uint32_t numBlocks = static_cast<uint32_t>(height * AUDIOLIB_ceilingDiv(width, vecSize));

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

   size_t width  = (this->_isInterleaved) ? this->_numChannels : this->_blockSize;
   size_t height = (this->_isInterleaved) ? this->_blockSize : this->_numChannels;

   // flatten 2D structure into 1D
   uint32_t numBlocks = static_cast<uint32_t>(height * AUDIOLIB_ceilingDiv(width, vecSize));

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
   se1Params.DIM1  = static_cast<int32_t>(this->_pKneeWidth_dB - this->_pThreshold_dB);
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

   // Fetch 1D attack and release coeff buffers with SA2, SA3; store 1D output gain buffer with SA1 (Use same SA
   // parameters for address generation pattern)
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
template <typename T> inline void Limiter<T>::TISP_limiter_gainComputer_exec(void *handle, T *pIn, T *pOut, T *pGain)
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
   T *pThresholdLocal    = static_cast<T *>(this->_pThreshold_dB);
   T *pKneeWidthLocal    = static_cast<T *>(this->_pKneeWidth_dB);
   T *pAttackCoeffLocal  = static_cast<T *>(this->_pAttackCoeff);
   T *pReleaseCoeffLocal = static_cast<T *>(this->_pReleaseCoeff);

   // Update SE1 parameters with correct dimension jump between buffer addresses
   se1Params.DIM1 = static_cast<int32_t>(pKneeWidthLocal - pThresholdLocal);

   T *pInLocal   = static_cast<T *>(pIn);
   T *pOutLocal  = static_cast<T *>(pOut);
   T *pGainLocal = static_cast<T *>(pGain);

   using vec             = typename c7x::make_full_vector<T>::type;
   uint32_t elementCount = c7x::element_count_of<vec>::value;

   size_t width  = (this->_isInterleaved) ? this->_numChannels : this->_blockSize;
   size_t height = (this->_isInterleaved) ? this->_blockSize : this->_numChannels;

   uint32_t numBlocks = static_cast<uint32_t>(AUDIOLIB_ceilingDiv(width, elementCount));

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
   vec vMakeUpGain   = vZero;
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
      __vpred cmpSoftKneePolicy = __negate(cmpHardKneePolicy);

      vSmoothedGain = vZero;
      vMakeUpGain   = vZero;

#pragma MUST_ITERATE(1, 65535, )
      for (size_t n = 0; n < height; n++) {

         // fetch input vector (in decibel)
         vec vIn = c7x::strm_eng<0, vec>::get_adv();

         // Determine knee policy predicate vectors
         __vpred cmpHardKneeThreshold = __and(__cmp_ge_pred(vIn, vThreshold), cmpHardKneePolicy);
         __vpred cmpSoftKneeThreshold = __and(__cmp_gt_pred(vIn, vSoftKneeEnd), cmpSoftKneePolicy);
         __vpred cmpHardKneeNone      = __and(__negate(cmpHardKneeThreshold), cmpHardKneePolicy);
         __vpred cmpSoftKneeNone      = __and(__cmp_lt_pred(vIn, vSoftKneeStart), cmpSoftKneePolicy);

         // Calculate soft knee region vector
         vec vRatio  = vIn - vThreshold + vKneeWidthHalfBand;
         vec vRatio2 = vRatio * vRatio;

         vec vSoftKnee = vIn - vRatio2 * vInvKneeWidthDoubleBand;

         // Default static characteristic is soft knee target region
         vec vStaticCharacteristic = vSoftKnee;

         // Soft Knee Above Knee End
         vStaticCharacteristic = __select(cmpSoftKneeThreshold, vThreshold, vStaticCharacteristic);

         // Soft Knee Below Knee Start
         vStaticCharacteristic = __select(cmpSoftKneeNone, vIn, vStaticCharacteristic);

         // Hard Knee Below Threshold
         vStaticCharacteristic = __select(cmpHardKneeNone, vIn, vStaticCharacteristic);

         // Hard Knee Above Threshold
         vStaticCharacteristic = __select(cmpHardKneeThreshold, vThreshold, vStaticCharacteristic);

         // Compute target gain
         vec vTargetGain = vStaticCharacteristic - vIn;

         // Determine smoothing coefficients
         __vpred cmpSmoothingMode = __cmp_le_pred(vTargetGain, vSmoothedGain);

         // Compute smoothed gains with both mode coefficients
         vec vSmoothedGainRelease = vReleaseCoeff * vSmoothedGain + vOneMinusReleaseCoeff * vTargetGain;
         vec vSmoothedGainAttack  = vAttackCoeff * vSmoothedGain + vOneMinusAttackCoeff * vTargetGain;

         vSmoothedGain = __select(cmpSmoothingMode, vSmoothedGainAttack, vSmoothedGainRelease);

         // Determine makeup gain
         __vpred cmpMakeUpGainMode = __cmp_eq_pred(vIn, vZero);
         vMakeUpGain               = __select(cmpMakeUpGainMode, vSmoothedGain - vStaticCharacteristic, vSmoothedGain);

         // Store smoothed gains
         __vpred vPredOut = c7x::strm_agen<0, vec>::get_vpred();
         vec    *vAddrOut = c7x::strm_agen<0, vec>::get_adv(pOutLocal);
         __vstore_pred(vPredOut, vAddrOut, vMakeUpGain);
      }

      // Store last of smoothed gains to gain buffer
      __vpred vPredGain = c7x::strm_agen<1, vec>::get_vpred();
      vec    *vAddrGain = c7x::strm_agen<1, vec>::get_adv(pGainLocal);
      __vstore_pred(vPredGain, vAddrGain, vMakeUpGain);
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

template <typename T>
Limiter<T>::Limiter(T          *pX,
                    T          *pG,
                    T          *pS,
                    T          *pY,
                    T          *pThreshold_dB,
                    T          *pKneeWidth_dB,
                    T          *pAttackCoeff,
                    T          *pReleaseCoeff,
                    size_t      numChannels,
                    size_t      blockSize,
                    size_t      pitchX,
                    size_t      pitchS,
                    size_t      pitchY,
                    bool        isInterleaved,
                    const char *nodeName,
                    uint32_t    nodeId,
                    ErrorCtxt  &errorCtx)
    : Limiter(pX,
              pG,
              pS,
              pY,
              pThreshold_dB,
              pKneeWidth_dB,
              pAttackCoeff,
              pReleaseCoeff,
              numChannels,
              blockSize,
              pitchX,
              pitchS,
              pitchY,
              isInterleaved,
              errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Limiter<float>;

} // namespace AUDIOLIB
} // namespace TISP
