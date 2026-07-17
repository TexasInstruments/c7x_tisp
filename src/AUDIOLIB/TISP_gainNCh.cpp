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
GainNCh<T>::GainNCh(T         *pX,
                    T         *pG,
                    T         *pY,
                    size_t     width,
                    size_t     height,
                    size_t     gHeight,
                    size_t     inPitchBytes,
                    size_t     outPitchBytes,
                    size_t     isInterleave,
                    ErrorCtxt &errorCtx)
    : _pX(pX), _pG(pG), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "width=%zu, height=%zu, gHeight=%zu, isInterleave=%zu", width, height, gHeight, isInterleave);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("GainNCh constructor", "input pointer pX is null");
   }
   else if (pG == nullptr) {
      errorCtx.setNullPointerError("GainNCh constructor", "gain pointer pG is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("GainNCh constructor", "output pointer pY is null");
   }
   else if (width == 0) {
      errorCtx.setInvalidParameterError("GainNCh constructor", "width must be greater than zero");
   }
   else if (height == 0) {
      errorCtx.setInvalidParameterError("GainNCh constructor", "height must be greater than zero");
   }
   else if (gHeight == 0) {
      errorCtx.setInvalidParameterError("GainNCh constructor", "gHeight must be greater than zero");
   }
   else if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("GainNCh constructor", "interleave must be either zero or one");
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
      /* Initialize data structures required by AUDIOLIB_Gainnch from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_gainNCh_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = isInterleave;

      _handle = make_unique_handle<char>(AUDIOLIB_gainNCh_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("GainNCh constructor", "failed to allocate gainNCh handle");
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

         AUDIOLIB_STATUS status = AUDIOLIB_gainNCh_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsGain,
                                                                    &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(status, "AUDIOLIB_gainNCh_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for gainNCh to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            status = AUDIOLIB_gainNCh_init(_handle.get(), &bufParamsIn, &bufParamsGain, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_gainNCh_init");
         }
      }
   }
}

template <typename T> void *GainNCh<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *GainNCh<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t GainNCh<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t GainNCh<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::GainNCh setAddr]
template <typename T> void GainNCh<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void GainNCh<T>::exec() noexcept { AUDIOLIB_gainNCh_exec(_handle.get(), _pX, _pG, _pY); }

template <typename T>
GainNCh<T>::GainNCh(T          *pX,
                    T          *pG,
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
    : GainNCh(pX, pG, pY, width, height, gHeight, inPitchBytes, outPitchBytes, isInterleave, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
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
         size_t expectedSize = _gHeight * sizeof(T);
         if (size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNCh::setParams", "Value size smaller than expected for kGainValue");
         }
         else {
            memcpy(_pG, value, expectedSize);
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
   if (value == nullptr) {
      errorCtx.setNullPointerError("GainNCh::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("GainNCh::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<GainNChParamId>(paramId)) {
      case GainNChParamId::kGainValue: {
         size_t expectedSize = _gHeight * sizeof(T);
         if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("GainNCh::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, _pG, expectedSize);
         }
         *size = expectedSize;
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
