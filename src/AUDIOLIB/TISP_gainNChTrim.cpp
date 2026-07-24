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
GainNChTrim<T>::GainNChTrim(T         *pX,
                            T         *pG,
                            T         *pM,
                            T         *pY,
                            size_t     width,
                            size_t     height,
                            size_t     gHeight,
                            size_t     inPitchBytes,
                            size_t     outPitchBytes,
                            size_t     isInterleave,
                            ErrorCtxt &errorCtx)
    : _pX(pX), _pG(pG), _pM(pM), _pY(pY), _bypassMode(0U)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "width=%zu, height=%zu, gHeight=%zu, isInterleave=%zu, masterGain=%.2f", width, height, gHeight,
                   isInterleave, pM ? *pM : 0.0f);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim constructor", "input pointer pX is null");
   }
   else if (pG == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim constructor", "gain pointer pG is null");
   }
   else if (pM == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim constructor", "master gain pointer pM is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("GainNChTrim constructor", "output pointer pY is null");
   }
   else if (width == 0) {
      errorCtx.setInvalidParameterError("GainNChTrim constructor", "width must be greater than zero");
   }
   else if (height == 0) {
      errorCtx.setInvalidParameterError("GainNChTrim constructor", "height must be greater than zero");
   }
   else if (gHeight == 0) {
      errorCtx.setInvalidParameterError("GainNChTrim constructor", "gHeight must be greater than zero");
   }
   else if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("GainNChTrim constructor", "interleave must be either zero or one");
   }
   else {
      _width         = width;
      _height        = height;
      _gHeight       = gHeight;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _isInterleave  = isInterleave;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Gainnchtrim from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_gainNChTrim_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = isInterleave;

      _handle = make_unique_handle<char>(AUDIOLIB_gainNChTrim_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("GainNChTrim constructor", "failed to allocate gainNChTrim handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;
         AUDIOLIB_bufParams1D_t bufParamsGain;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);

         bufParamsGain.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsGain.dim_x     = static_cast<uint32_t>(gHeight);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status = AUDIOLIB_gainNChTrim_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsGain,
                                                                        &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(status, "AUDIOLIB_gainNChTrim_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for gainNChTrim to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            status =
                AUDIOLIB_gainNChTrim_init(_handle.get(), &bufParamsIn, &bufParamsGain, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_gainNChTrim_init");
         }
      }
   }
}

template <typename T> void *GainNChTrim<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *GainNChTrim<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t GainNChTrim<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t GainNChTrim<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Gainnchtrim setAddr]
template <typename T> void GainNChTrim<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<T *>(addr.src[0]);
   }
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<T *>(addr.src[0]);
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void GainNChTrim<T>::bypassCopy() noexcept
{
   const size_t rowBytes = _width * sizeof(T);

   if (_pX != _pY) {
      if ((_inPitchBytes == rowBytes) && (_outPitchBytes == rowBytes)) {
         (void) memcpy(_pY, _pX, rowBytes * _height);
      }
      else {
         uint8_t       *pDst = reinterpret_cast<uint8_t *>(_pY);
         const uint8_t *pSrc = reinterpret_cast<const uint8_t *>(_pX);

         for (size_t i = 0; i < _height; i++) {
            (void) memcpy(&pDst[i * _outPitchBytes], &pSrc[i * _inPitchBytes], rowBytes); // NOLINT(MISRA.PTR.ARITH)
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
      AUDIOLIB_gainNChTrim_exec(_handle.get(), _pX, _pG, _pM, _pY);
   }
}

template <typename T>
GainNChTrim<T>::GainNChTrim(T          *pX,
                            T          *pG,
                            T          *pM,
                            T          *pY,
                            size_t      width,
                            size_t      height,
                            size_t      gHeight,
                            size_t      inPitchBytes,
                            size_t      outPitchBytes,
                            size_t      isInterleave,
                            const char *nodeName,
                            uint32_t    nodeId,
                            ErrorCtxt  &errorCtx)
    : GainNChTrim(pX, pG, pM, pY, width, height, gHeight, inPitchBytes, outPitchBytes, isInterleave, errorCtx)
{
   (void) pX;
   (void) pG;
   (void) pM;
   (void) pY;
   (void) width;
   (void) height;
   (void) gHeight;
   (void) inPitchBytes;
   (void) outPitchBytes;
   (void) isInterleave;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
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
         size_t expectedSize = _gHeight * sizeof(T);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNChTrim::setParams",
                                              "Value size smaller than expected for kGainValues");
         }
         else {
            memcpy(_pG, value, expectedSize);
         }
         break;
      }
      case GainNChTrimParamId::kMasterGain: {
         if (size < sizeof(T)) {
            errorCtx.setInvalidParameterError("GainNChTrim::setParams",
                                              "Value size smaller than expected for kMasterGain");
         }
         else {
            memcpy(_pM, value, sizeof(T));
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
         size_t expectedSize = _gHeight * sizeof(T);
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNChTrim::getParams",
                                              "Value size smaller than expected for kGainValues");
         }
         else {
            memcpy(value, _pG, expectedSize);
         }
         *size = expectedSize;
         break;
      }
      case GainNChTrimParamId::kMasterGain: {
         if (*size < sizeof(T)) {
            errorCtx.setInvalidParameterError("GainNChTrim::getParams",
                                              "Value size smaller than expected for kMasterGain");
         }
         else {
            memcpy(value, _pM, sizeof(T));
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
