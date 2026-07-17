// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_interleave.hpp>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [TISP::Interleave constructor]

template <typename T>
Interleave<T>::Interleave(T         *pX,
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
      errorCtx.setNullPointerError("Interleave constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Interleave constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Interleave constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("Interleave constructor", "height must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width         = width;
      _height        = height;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 1;
      _nodeInfo.bufParams.widthOut[0]        = height;
      _nodeInfo.bufParams.heightOut[0]       = width;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /************************************************************************/
      /* Initialize data structures required by DSPLIB_interleave from DSPLIB */
      /************************************************************************/

      DSPLIB_interleaveInitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_interleave_getHandleSize(&kerInitArgs));

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
         errorCtx.setMemoryError("Interleave constructor", "failed to allocate interleave handle");
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_STATUS checkRet =
             DSPLIB_interleave_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_interleave_init_checkParams");

         /******************************************************************************/
         /* Call init function from DSPLIB to for interleave to set SE/SA params */
         /******************************************************************************/
         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS initRet = DSPLIB_interleave_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "DSPLIB_interleave_init");
         }
      }
   }
}

template <typename T>
Interleave<T>::Interleave(T          *pX,
                          T          *pY,
                          size_t      width,
                          size_t      height,
                          size_t      inPitchBytes,
                          size_t      outPitchBytes,
                          const char *nodeName,
                          uint32_t    nodeId,
                          ErrorCtxt  &errorCtx)
    : Interleave(pX, pY, width, height, inPitchBytes, outPitchBytes, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Interleave<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Interleave<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Interleave<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Interleave<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Interleave setAddr]
template <typename T> void Interleave<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Interleave<T>::exec() noexcept { DSPLIB_interleave_exec(_handle.get(), _pX, _pY); }

template class Interleave<float>;
template class Interleave<double>;
template class Interleave<int8_t>;
template class Interleave<uint8_t>;
template class Interleave<int16_t>;
template class Interleave<uint16_t>;
template class Interleave<int32_t>;
template class Interleave<uint32_t>;
template class Interleave<int64_t>;
template class Interleave<uint64_t>;

} // namespace DSPLIB
} // namespace TISP
