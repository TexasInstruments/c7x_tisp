// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_undB20.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Undb20 constructor]

template <typename T>
Undb20<T>::Undb20(T         *pX,
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
      errorCtx.setNullPointerError("Undb20 constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Undb20 constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Undb20 constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("Undb20 constructor", "height must be greater than zero");
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
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Undb20 from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_undB20_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(AUDIOLIB_undB20_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Undb20 constructor", "failed to allocate undB20 handle");
      }

      if (errorCtx.isSuccess()) {

         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_undB20_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_undB20_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for undB20 to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_undB20_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_undB20_init");
         }
      }
   }
}

template <typename T> void *Undb20<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Undb20<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Undb20<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Undb20<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Undb20 setAddr]
template <typename T> void Undb20<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Undb20<T>::exec() noexcept { AUDIOLIB_undB20_exec(_handle.get(), _pX, _pY); }

template <typename T>
Undb20<T>::Undb20(T          *pX,
                  T          *pY,
                  size_t      width,
                  size_t      height,
                  size_t      inPitchBytes,
                  size_t      outPitchBytes,
                  const char *nodeName,
                  uint32_t    nodeId,
                  ErrorCtxt  &errorCtx)
    : Undb20(pX, pY, width, height, inPitchBytes, outPitchBytes, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Undb20<float>;

} // namespace AUDIOLIB
} // namespace TISP
