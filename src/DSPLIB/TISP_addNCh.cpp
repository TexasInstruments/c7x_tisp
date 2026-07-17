// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_addNCh.hpp>

namespace TISP {
namespace DSPLIB {

/* AddNch constructor */

template <typename T>
AddNch<T>::AddNch(T         *pX,
                  T         *pY,
                  size_t     width,
                  size_t     height,
                  size_t     inputs,
                  size_t     inPitchBytesY,
                  size_t     inPitchBytesZ,
                  ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{

   /* Initialize _params with the node parameters */
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "widthIn=%zu,heightIn=%zu,inputs=%zu", width,
                   height, inputs);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("AddNch constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("AddNch constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("AddNch constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("AddNch constructor", "height must be greater than zero");
   }
   if (inputs == 0) {
      errorCtx.setInvalidParameterError("AddNch constructor", "inputs must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytesY;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = inPitchBytesY;

      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_addNCh from DSPLIB */
      /***********************************************************************/

      DSPLIB_addNCh_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_addNCh_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("AddNch constructor", "failed to allocate addNCh handle");
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_bufParams3D_t bufParamsIn;
         DSPLIB_bufParams2D_t bufParamsOut;

         bufParamsIn.data_type = DSPLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.dim_z     = static_cast<uint32_t>(inputs);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytesY);
         bufParamsIn.stride_z  = static_cast<int32_t>(inPitchBytesZ);

         bufParamsOut.data_type = DSPLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(inPitchBytesY);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         DSPLIB_STATUS checkRet =
             DSPLIB_addNCh_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_addNCh_init_checkParams");
         /******************************************************************************/
         /* Call init function from DSPLIB to for addNCh to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS initRet = DSPLIB_addNCh_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "DSPLIB_addNCh_init");
         }
      }
   }
}

/* AddNch setAddr */

template <typename T> void *AddNch<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *AddNch<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t AddNch<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t AddNch<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void AddNch<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void AddNch<T>::exec() noexcept { DSPLIB_addNCh_exec(_handle.get(), _pX, _pY); }

template <typename T>
AddNch<T>::AddNch(T          *pX,
                  T          *pY,
                  size_t      width,
                  size_t      height,
                  size_t      inputs,
                  size_t      inPitchBytesY,
                  size_t      inPitchBytesZ,
                  const char *nodeName,
                  uint32_t    nodeId,
                  ErrorCtxt  &errorCtx)
    : AddNch(pX, pY, width, height, inputs, inPitchBytesY, inPitchBytesZ, errorCtx)
{
   (void) pX;
   (void) pY;
   (void) width;
   (void) height;
   (void) inputs;
   (void) inPitchBytesY;
   (void) inPitchBytesZ;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class AddNch<float>;
template class AddNch<double>;

} // namespace DSPLIB
} // namespace TISP
