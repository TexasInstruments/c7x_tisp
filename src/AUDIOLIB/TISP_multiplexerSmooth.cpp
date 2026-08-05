// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include "common/AUDIOLIB_types.h"
#include "drivers/hw_include/csl_types.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <DSPLIB/TISP_DSPLIB_types.hpp>
#include <TISP_multiplexerSmooth.hpp>
#include <cstdint>

namespace TISP {
namespace AUDIOLIB {

#define TISP_MUX_SMOOTH_NPARAMS (2)

/* MultiplexerSmooth constructor */

template <typename T>
MultiplexerSmooth<T>::MultiplexerSmooth(T         *pX0,
                                        T         *pSmoothedSampleBuff,
                                        T         *pGainFadeIn,
                                        T         *pGainFadeOut,
                                        T         *pY,
                                        T         *pWs,
                                        uint32_t   maxInputs,
                                        size_t     widthIn,
                                        size_t     heightIn,
                                        size_t     inPitchBytes,
                                        size_t     outPitchBytes,
                                        size_t     isInterleave,
                                        size_t     currentIdx,
                                        size_t     sampleRate,
                                        float      fadeTime,
                                        ErrorCtxt &errorCtx)
    : _pX0(pX0), _pWs(pWs), _pSmoothedSampleBuff(pSmoothedSampleBuff), _pGainFadeIn(pGainFadeIn),
      _pGainFadeOut(pGainFadeOut), _pY(pY), _maxInputs(maxInputs), _historyBufSize(0)
{

   /* Initialize _params with the node parameters */
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "widthIn=%zu,heightIn=%zu,isInterleave=%zu",
                   widthIn, heightIn, isInterleave);

   // Input validation checks
   if (pX0 == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth constructor", "input pointer pX0 is null");
   }
   else if (pSmoothedSampleBuff == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth constructor", "input pointer pSmoothedSampleBuff is null");
   }
   else if (pGainFadeIn == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth constructor", "input pointer pGainFadeIn is null");
   }
   else if (pGainFadeOut == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth constructor", "input pointer pGainFadeOut is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth constructor", "output pointer pY is null");
   }
   else if (pWs == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth constructor", "input pointer pWs is null");
   }
   else if (maxInputs == 0) {
      errorCtx.setInvalidParameterError("MultiplexerSmooth constructor", "maxInputs must be greater than zero");
   }
   else if (widthIn == 0) {
      errorCtx.setInvalidParameterError("MultiplexerSmooth constructor", "width must be greater than zero");
   }
   else if (heightIn == 0) {
      errorCtx.setInvalidParameterError("MultiplexerSmooth constructor", "height must be greater than zero");
   }
   else if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("MultiplexerSmooth constructor", "isInterleave must be either zero or one");
   }
   else if (sampleRate == 0) {
      errorCtx.setInvalidParameterError("MultiplexerSmooth constructor", "sampleRate must be greater than zero");
   }
   else if (fadeTime < 0) {
      errorCtx.setInvalidParameterError("MultiplexerSmooth constructor", "fadeTime must be greater than zero");
   }
   else {
      _width           = widthIn;
      _height          = heightIn;
      _inPitchBytes    = inPitchBytes;
      _outPitchBytes   = outPitchBytes;
      _isInterleave    = isInterleave;
      _smoothedSamples = static_cast<size_t>(sampleRate * fadeTime / 1000);

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      _nodeInfo.bufParams.widthIn[0]        = widthIn;
      _nodeInfo.bufParams.heightIn[0]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      _nodeInfo.bufParams.widthOut[0]        = widthIn;
      _nodeInfo.bufParams.heightOut[0]       = heightIn;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Multiplexersmooth from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_crossfade_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = isInterleave;

      DSPLIB_blkCopy2DInitArgs kerInitArgsD;
      kerInitArgsD.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handleCrossFader = make_unique_handle<char>(AUDIOLIB_crossfade_getHandleSize(&kerInitArgs));
      _handleBlkCopy    = make_unique_handle<char>(DSPLIB_blkCopy2D_getHandleSize(&kerInitArgsD));

      AUDIOLIB_bufParams2D_t bufParamsCrosFadeIn0;
      AUDIOLIB_bufParams2D_t bufParamsCrosFadeIn1;
      AUDIOLIB_bufParams2D_t bufParamsCrosFadeIOut;
      AUDIOLIB_bufParams1D_t bufParamsCrosFadeGain0;
      AUDIOLIB_bufParams1D_t bufParamsCrosFadeGain1;

      DSPLIB_bufParams2D_t bufParamsIn0D;
      DSPLIB_bufParams2D_t bufParamsOutD;

      /* Set the default source */
      _curentIdx = (currentIdx >= _maxInputs) ? 0 : currentIdx;
      _prevIdx   = _curentIdx;

      if (_smoothedSamples == 0) {

         bufParamsIn0D.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
         bufParamsIn0D.dim_x     = static_cast<uint32_t>(widthIn);
         bufParamsIn0D.dim_y     = static_cast<uint32_t>(heightIn);
         bufParamsIn0D.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOutD.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
         bufParamsOutD.dim_x     = static_cast<uint32_t>(widthIn);
         bufParamsOutD.dim_y     = static_cast<uint32_t>(heightIn);
         bufParamsOutD.stride_y  = static_cast<int32_t>(outPitchBytes);
      }
      else {
         if (isInterleave) {
            if (heightIn <= _smoothedSamples) {
               _smoothedSamples = heightIn;
            }
            bufParamsIn0D.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
            bufParamsIn0D.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsIn0D.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsIn0D.stride_y  = static_cast<int32_t>(inPitchBytes);

            bufParamsOutD.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
            bufParamsOutD.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsOutD.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsOutD.stride_y  = static_cast<int32_t>(outPitchBytes);

            bufParamsCrosFadeIn0.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeIn0.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsCrosFadeIn0.dim_y     = static_cast<uint32_t>(_smoothedSamples);
            bufParamsCrosFadeIn0.stride_y  = static_cast<int32_t>(inPitchBytes);

            bufParamsCrosFadeIn1.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeIn1.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsCrosFadeIn1.dim_y     = static_cast<uint32_t>(_smoothedSamples);
            bufParamsCrosFadeIn1.stride_y  = static_cast<int32_t>(inPitchBytes);

            bufParamsCrosFadeGain0.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeGain0.dim_x     = static_cast<uint32_t>(_smoothedSamples);

            bufParamsCrosFadeGain1.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeGain1.dim_x     = static_cast<uint32_t>(_smoothedSamples);

            bufParamsCrosFadeIOut.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeIOut.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsCrosFadeIOut.dim_y     = static_cast<uint32_t>(_smoothedSamples);
            bufParamsCrosFadeIOut.stride_y  = static_cast<int32_t>(outPitchBytes);
         }
         else {
            if (widthIn <= _smoothedSamples) {
               _smoothedSamples = widthIn;
            }
            bufParamsIn0D.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
            bufParamsIn0D.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsIn0D.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsIn0D.stride_y  = static_cast<int32_t>(inPitchBytes);

            bufParamsOutD.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
            bufParamsOutD.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsOutD.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsOutD.stride_y  = static_cast<int32_t>(outPitchBytes);

            bufParamsCrosFadeIn0.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeIn0.dim_x     = static_cast<uint32_t>(_smoothedSamples);
            bufParamsCrosFadeIn0.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsCrosFadeIn0.stride_y  = static_cast<int32_t>(_smoothedSamples * sizeof(T));

            bufParamsCrosFadeIn1.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeIn1.dim_x     = static_cast<uint32_t>(_smoothedSamples);
            bufParamsCrosFadeIn1.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsCrosFadeIn1.stride_y  = static_cast<int32_t>(_smoothedSamples * sizeof(T));

            bufParamsCrosFadeGain0.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeGain0.dim_x     = static_cast<uint32_t>(_smoothedSamples);

            bufParamsCrosFadeGain1.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeGain1.dim_x     = static_cast<uint32_t>(_smoothedSamples);

            bufParamsCrosFadeIOut.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsCrosFadeIOut.dim_x     = static_cast<uint32_t>(_smoothedSamples);
            bufParamsCrosFadeIOut.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsCrosFadeIOut.stride_y  = static_cast<int32_t>(_smoothedSamples * sizeof(T));
         }
      }

      /******************************************************************************/
      /* Validate parameters before init                                            */
      /******************************************************************************/

      if (_smoothedSamples != 0) {
         AUDIOLIB_STATUS status = AUDIOLIB_crossfade_init_checkParams(
             _handleCrossFader.get(), &bufParamsCrosFadeIn0, &bufParamsCrosFadeIn1, &bufParamsCrosFadeGain0,
             &bufParamsCrosFadeGain1, &bufParamsCrosFadeIOut, &kerInitArgs);
         errorCtx.remapXLIBError(status, "AUDIOLIB_crossfade_init_checkParams");
      }

      /******************************************************************************/
      /* Validate parameters before init                                            */
      /******************************************************************************/
      if (errorCtx.isSuccess()) {
         DSPLIB_STATUS status =
             DSPLIB_blkCopy2D_init_checkParams(_handleBlkCopy.get(), &bufParamsIn0D, &bufParamsOutD, &kerInitArgsD);
         errorCtx.remapXLIBError(status, "DSPLIB_blkCopy2D_init_checkParams");

         if (errorCtx.isSuccess()) {
            status = DSPLIB_blkCopy2D_init(_handleBlkCopy.get(), &bufParamsIn0D, &bufParamsOutD, &kerInitArgsD);
            errorCtx.remapXLIBError(status, "DSPLIB_blkCopy2D_init");
         }
      }

      /******************************************************************************/
      /* Call init function from AUDIOLIB to for multiplexerSmooth to set SE/SA params */
      /******************************************************************************/
      if (errorCtx.isSuccess()) {
         if (_smoothedSamples != 0) {
            AUDIOLIB_STATUS status = AUDIOLIB_crossfade_init(
                _handleCrossFader.get(), &bufParamsCrosFadeIn0, &bufParamsCrosFadeIn1, &bufParamsCrosFadeGain0,
                &bufParamsCrosFadeGain1, &bufParamsCrosFadeIOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_crossfade_init");

            _historyBufSize = heightIn * _smoothedSamples * sizeof(T);

            if (errorCtx.isSuccess()) {
               cleanInternalBuffers();
            }
         }
      }
   }
}

//! [TISP::MultiplexerSmooth setParams]
template <typename T>
bool MultiplexerSmooth<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<MultiplexersmoothParamId>(paramId)) {
      case MultiplexersmoothParamId::kSrcIndex: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("MultiplexerSmooth::setParams",
                                              "Value size smaller than expected for kSrcIndex");
         }
         else {
            uint32_t paramValue = 0;
            memcpy(&paramValue, value, sizeof(uint32_t));
            if (paramValue < _maxInputs) {
               _curentIdx = paramValue;
            }
            else {
               errorCtx.setInvalidParameterError("MultiplexerSmooth::setParams", "srcIndex out of bounds");
            }
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("MultiplexerSmooth::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool MultiplexerSmooth<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("MultiplexerSmooth::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<MultiplexersmoothParamId>(paramId)) {
      case MultiplexersmoothParamId::kSrcIndex: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("MultiplexerSmooth::getParams", "Provided buffer size is too small");
         }
         else {
            uint32_t paramValue = static_cast<uint32_t>(_curentIdx);
            memcpy(value, &paramValue, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("MultiplexerSmooth::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> void *MultiplexerSmooth<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX0) : nullptr;
}

template <typename T> void *MultiplexerSmooth<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t MultiplexerSmooth<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t MultiplexerSmooth<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::MultiplexerSmooth setAddr]
template <typename T> void MultiplexerSmooth<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX0 = static_cast<T *>(addr.src[0]) + (_curentIdx * _height * _inPitchBytes / sizeof(T));
   }
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX0 = static_cast<T *>(addr.src[0]) + (_curentIdx * _height * _inPitchBytes / sizeof(T));
      _pY  = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void MultiplexerSmooth<T>::exec() noexcept
{
   if ((_prevIdx == _curentIdx) || (_smoothedSamples == 0)) {
      /* If the source index is not changed, just do a block copy */
      DSPLIB_blkCopy2D_exec(_handleBlkCopy.get(), _pX0, _pY);
   }
   else {
      /* Update the _prevIdx with _curentIdx */
      _prevIdx = _curentIdx;

      /* Do smoothing */
      if (0 == _isInterleave) {
         if (_width <= _smoothedSamples) {
            /* Full frame to be faded */
            AUDIOLIB_crossfade_exec(_handleCrossFader.get(), _pSmoothedSampleBuff, _pX0, _pGainFadeOut, _pGainFadeIn,
                                    _pY);
         }
         else {
            size_t nSize          = (_height * _smoothedSamples);
            T     *_pInSmooth     = _pWs;
            T     *_pOutCrossFade = &_pInSmooth[nSize]; // NOLINT(MISRA.PTR.ARITH)

            /* Copy _smoothedSamples samples from input to do fading in */
            for (size_t j = 0; j < _height; j++) {
               for (size_t i = 0; i < _smoothedSamples; i++) {
                  _pInSmooth[j * _smoothedSamples + i] = _pX0[j * _width + i];
               }
            }

            /* Do cross fading */
            AUDIOLIB_crossfade_exec(_handleCrossFader.get(), _pSmoothedSampleBuff, _pInSmooth, _pGainFadeOut,
                                    _pGainFadeIn, _pOutCrossFade);

            /* Copy result of crossfade back to actual output buffer */
            for (size_t j = 0; j < _height; j++) {
               for (size_t i = 0; i < _smoothedSamples; i++) {
                  _pY[j * _width + i] = _pOutCrossFade[j * _smoothedSamples + i];
               }
            }

            /* Copy the samples for which crossfading not done to output buffer */
            for (size_t j = 0; j < _height; j++) {
               for (size_t i = _smoothedSamples; i < _width; i++) {
                  _pY[j * _width + i] = _pX0[j * _width + i];
               }
            }
         } /* end of width check */
      }    /* end of non-interleaved branch */
      else {
         if (_height <= _smoothedSamples) {
            /* Full frame to be faded */
            AUDIOLIB_crossfade_exec(_handleCrossFader.get(), _pSmoothedSampleBuff, _pX0, _pGainFadeOut, _pGainFadeIn,
                                    _pY);
         }
         else {
            size_t nSize          = (_width * _smoothedSamples);
            T     *_pInSmooth     = _pWs;
            T     *_pOutCrossFade = &_pInSmooth[nSize]; // NOLINT(MISRA.PTR.ARITH)

            for (size_t j = 0; j < _smoothedSamples; j++) {
               for (size_t i = 0; i < _width; i++) {
                  _pInSmooth[j * _width + i] = _pX0[j * _width + i];
               }
            }

            /* Do cross fading */
            AUDIOLIB_crossfade_exec(_handleCrossFader.get(), _pSmoothedSampleBuff, _pInSmooth, _pGainFadeOut,
                                    _pGainFadeIn, _pOutCrossFade);

            /* Copy result of crossfade back to actual output buffer */
            for (size_t j = 0; j < _smoothedSamples; j++) {
               for (size_t i = 0; i < _width; i++) {
                  _pY[j * _width + i] = _pOutCrossFade[j * _width + i];
               }
            }

            /* Copy the samples for which crossfading not done to output buffer */
            for (size_t j = _smoothedSamples; j < _height; j++) {
               for (size_t i = 0; i < _width; i++) {
                  _pY[j * _width + i] = _pX0[j * _width + i];
               }
            }
         } /* end of height check */
      }    /* end of interleaved branch */
   }

   /* Update the smoothing history buffer */
   if (_isInterleave) {
      for (size_t j = 0; j < _smoothedSamples; j++) {
         for (size_t i = 0; i < _width; i++) {
            _pSmoothedSampleBuff[j * _width + i] = _pX0[j * _width + i];
         }
      }
   }
   else {
      for (size_t j = 0; j < _height; j++) {
         for (size_t i = 0; i < _smoothedSamples; i++) {
            _pSmoothedSampleBuff[j * _smoothedSamples + i] = _pX0[j * _width + i];
         }
      }
   }
}

template <typename T>
MultiplexerSmooth<T>::MultiplexerSmooth(T          *pX0,
                                        T          *pSmoothedSampleBuff,
                                        T          *pGainFadeIn,
                                        T          *pGainFadeOut,
                                        T          *pY,
                                        T          *pWs,
                                        uint32_t    maxInputs,
                                        size_t      widthIn,
                                        size_t      heightIn,
                                        size_t      inPitchBytes,
                                        size_t      outPitchBytes,
                                        size_t      isInterleave,
                                        size_t      currentIdx,
                                        size_t      sampleRate,
                                        float       fadeTime,
                                        const char *nodeName,
                                        uint32_t    nodeId,
                                        ErrorCtxt  &errorCtx)
    : MultiplexerSmooth(pX0,
                        pSmoothedSampleBuff,
                        pGainFadeIn,
                        pGainFadeOut,
                        pY,
                        pWs,
                        maxInputs,
                        widthIn,
                        heightIn,
                        inPitchBytes,
                        outPitchBytes,
                        isInterleave,
                        currentIdx,
                        sampleRate,
                        fadeTime,
                        errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void MultiplexerSmooth<T>::cleanInternalBuffers() noexcept
{
   if (_pSmoothedSampleBuff != nullptr && _historyBufSize > 0) {
      std::memset(_pSmoothedSampleBuff, 0, _historyBufSize);
   }
}

template class MultiplexerSmooth<float>;
template class MultiplexerSmooth<double>;

} // namespace AUDIOLIB
} // namespace TISP
