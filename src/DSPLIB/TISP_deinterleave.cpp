// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_deinterleave.hpp>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [TISP::Deinterleave constructor]

template <typename T>
Deinterleave<T>::Deinterleave(T         *pX,
                              T         *pY,
                              size_t     width,
                              size_t     height,
                              size_t     inPitchBytes,
                              size_t     outPitchBytes,
                              ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", width, height);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Deinterleave constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Deinterleave constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Deinterleave constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("Deinterleave constructor", "height must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width         = width;
      _height        = height;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 1;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = height;
      _nodeInfo.bufParams.heightOut[0]       = width;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_deinterleave from DSPLIB */
      /***********************************************************************/

      DSPLIB_deinterleaveInitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_deinterleave_getHandleSize(&kerInitArgs));

      DSPLIB_bufParams2D_t bufParamsIn;
      DSPLIB_bufParams2D_t bufParamsOut;

      bufParamsIn.data_type = DSPLIB_element_dType<T>::value;
      bufParamsIn.dim_x     = static_cast<uint32_t>(width);
      bufParamsIn.dim_y     = static_cast<uint32_t>(height);
      bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

      bufParamsOut.data_type = DSPLIB_element_dType<T>::value;
      bufParamsOut.dim_x     = static_cast<uint32_t>(height);
      bufParamsOut.dim_y     = static_cast<uint32_t>(width);
      bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);

      if (!_handle) {
         errorCtx.setMemoryError("Deinterleave constructor", "failed to allocate deinterleave handle");
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_STATUS checkRet =
             DSPLIB_deinterleave_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_deinterleave_init_checkParams");

         /******************************************************************************/
         /* Call init function from DSPLIB to for deinterleave to set SE/SA params */
         /******************************************************************************/
         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS initRet = DSPLIB_deinterleave_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "DSPLIB_deinterleave_init");
         }
      }
   }
}

template <typename T>
Deinterleave<T>::Deinterleave(T          *pX,
                              T          *pY,
                              size_t      width,
                              size_t      height,
                              size_t      inPitchBytes,
                              size_t      outPitchBytes,
                              const char *nodeName,
                              uint32_t    nodeId,
                              ErrorCtxt  &errorCtx)
    : Deinterleave(pX, pY, width, height, inPitchBytes, outPitchBytes, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Deinterleave<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Deinterleave<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Deinterleave<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Deinterleave<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Deinterleave setAddr]
template <typename T> void Deinterleave<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Deinterleave<T>::exec() noexcept { DSPLIB_deinterleave_exec(_handle.get(), _pX, _pY); }

template class Deinterleave<float>;
template class Deinterleave<double>;
template class Deinterleave<int8_t>;
template class Deinterleave<uint8_t>;
template class Deinterleave<int16_t>;
template class Deinterleave<uint16_t>;
template class Deinterleave<int32_t>;
template class Deinterleave<uint32_t>;
template class Deinterleave<int64_t>;
template class Deinterleave<uint64_t>;

} // namespace DSPLIB
} // namespace TISP
