// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_matTrans.hpp>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [DSPLIB_MatTrans constructor]

template <typename T>
MatTrans<T>::MatTrans(T         *pX,
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
      errorCtx.setNullPointerError("MatTrans constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("MatTrans constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("MatTrans constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("MatTrans constructor", "height must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

      // Initialize bufParams with node input and output buffer parameters. MatTrans
      // swaps width/height between input and output (M x N input -> N x M output).
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = height;
      _nodeInfo.bufParams.heightOut[0]       = width;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_matrTrans from DSPLIB */
      /***********************************************************************/

      DSPLIB_matTransInitArgs kerInitArgs;
      kerInitArgs.dimX      = width;
      kerInitArgs.dimY      = height;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_matTrans_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("MatTrans constructor", "failed to allocate matTrans handle");
      }

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

      if (errorCtx.isSuccess()) {
         DSPLIB_STATUS checkRet =
             DSPLIB_matTrans_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_matTrans_init_checkParams");

         /******************************************************************************/
         /* Call init function from DSPLIB to for matrix transpose to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS initRet = DSPLIB_matTrans_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "DSPLIB_matTrans_init");
         }
      }
   }
}

template <typename T>
MatTrans<T>::MatTrans(T          *pX,
                      T          *pY,
                      size_t      width,
                      size_t      height,
                      size_t      inPitchBytes,
                      size_t      outPitchBytes,
                      const char *nodeName,
                      uint32_t    nodeId,
                      ErrorCtxt  &errorCtx)
    : MatTrans(pX, pY, width, height, inPitchBytes, outPitchBytes, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

//! [DSPLIB_MatTrans constructor]

template <typename T> void *MatTrans<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *MatTrans<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t MatTrans<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t MatTrans<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void MatTrans<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void MatTrans<T>::exec() noexcept { DSPLIB_matTrans_exec(_handle.get(), _pX, _pY); }

template class MatTrans<float>;
template class MatTrans<double>;

} // namespace DSPLIB
} // namespace TISP
