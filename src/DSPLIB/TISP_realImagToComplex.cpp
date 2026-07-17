// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_realImagToComplex.hpp>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [TISP::Realimagtocomplex constructor]

template <typename T>
RealimagToComplex<T>::RealimagToComplex(T         *pIn0,
                                        T         *pIn1,
                                        T         *pOut,
                                        size_t     widthIn,
                                        size_t     heightIn,
                                        size_t     widthOut,
                                        size_t     heightOut,
                                        size_t     inPitchBytes,
                                        size_t     outPitchBytes,
                                        ErrorCtxt &errorCtx)
    : _pIn0(pIn0), _pIn1(pIn1), _pOut(pOut)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "widthIn=%zu,heightIn=%zu,widthOut=%zu,heightOut=%zu", widthIn, heightIn, widthOut, heightOut);

   // Input validation checks
   if (pIn0 == nullptr) {
      errorCtx.setNullPointerError("RealimagToComplex constructor", "input pointer pIn0 is null");
   }
   if (pIn1 == nullptr) {
      errorCtx.setNullPointerError("RealimagToComplex constructor", "input pointer pIn1 is null");
   }
   if (pOut == nullptr) {
      errorCtx.setNullPointerError("RealimagToComplex constructor", "output pointer pOut is null");
   }
   if (widthIn == 0) {
      errorCtx.setInvalidParameterError("RealimagToComplex constructor", "widthIn must be greater than zero");
   }
   if (heightIn == 0) {
      errorCtx.setInvalidParameterError("RealimagToComplex constructor", "heightIn must be greater than zero");
   }
   if (widthOut == 0) {
      errorCtx.setInvalidParameterError("RealimagToComplex constructor", "widthOut must be greater than zero");
   }
   if (heightOut == 0) {
      errorCtx.setInvalidParameterError("RealimagToComplex constructor", "heightOut must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _widthIn       = widthIn;
      _heightIn      = heightIn;
      _widthOut      = widthOut;
      _heightOut     = heightOut;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;

      // Initialize bufParams with node input and output buffer parameters. This node
      // takes two real-valued inputs (real, imaginary) of identical geometry and
      // produces one complex output.
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = widthIn;
      _nodeInfo.bufParams.heightIn[0]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;
      _nodeInfo.bufParams.widthIn[1]        = widthIn;
      _nodeInfo.bufParams.heightIn[1]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[1]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = widthOut;
      _nodeInfo.bufParams.heightOut[0]       = heightOut;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_matrTrans from DSPLIB */
      /***********************************************************************/

      DSPLIB_realImagToComplex_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_realImagToComplex_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("RealimagToComplex constructor", "failed to allocate realImagToComplex handle");
      }

      if (errorCtx.isSuccess()) {

         DSPLIB_bufParams2D_t bufParamsIn0;
         DSPLIB_bufParams2D_t bufParamsIn1;
         DSPLIB_bufParams2D_t bufParamsOut;

         bufParamsIn0.data_type = DSPLIB_element_dType<T>::value;
         bufParamsIn0.dim_x     = static_cast<uint32_t>(widthIn);
         bufParamsIn0.dim_y     = static_cast<uint32_t>(heightIn);
         bufParamsIn0.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsIn1.data_type = DSPLIB_element_dType<T>::value;
         bufParamsIn1.dim_x     = static_cast<uint32_t>(widthIn);
         bufParamsIn1.dim_y     = static_cast<uint32_t>(heightIn);
         bufParamsIn1.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOut.data_type = DSPLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(widthOut);
         bufParamsOut.dim_y     = static_cast<uint32_t>(heightOut);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);

         /******************************************************************************/
         /* Call init function from DSPLIB to for realImagToComplex to set SE/SA params       */
         /******************************************************************************/

         DSPLIB_STATUS checkRet = DSPLIB_realImagToComplex_init_checkParams(_handle.get(), &bufParamsIn0, &bufParamsIn1,
                                                                            &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_realImagToComplex_init_checkParams");

         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS initRet =
                DSPLIB_realImagToComplex_init(_handle.get(), &bufParamsIn0, &bufParamsIn1, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "DSPLIB_realImagToComplex_init");
         }
      }
   }
}

template <typename T>
RealimagToComplex<T>::RealimagToComplex(T          *pIn0,
                                        T          *pIn1,
                                        T          *pOut,
                                        size_t      widthIn,
                                        size_t      heightIn,
                                        size_t      widthOut,
                                        size_t      heightOut,
                                        size_t      inPitchBytes,
                                        size_t      outPitchBytes,
                                        const char *nodeName,
                                        uint32_t    nodeId,
                                        ErrorCtxt  &errorCtx)
    : RealimagToComplex(pIn0, pIn1, pOut, widthIn, heightIn, widthOut, heightOut, inPitchBytes, outPitchBytes, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *RealimagToComplex<T>::getInputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   switch (index) {
   case 0:
      result = static_cast<void *>(_pIn0);
      break;
   case 1:
      result = static_cast<void *>(_pIn1);
      break;
   default:
      result = nullptr;
      break;
   }
   return result;
}

template <typename T> void *RealimagToComplex<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pOut) : nullptr;
}

template <typename T> size_t RealimagToComplex<T>::getNumInputs() const noexcept { return 2; }

template <typename T> size_t RealimagToComplex<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Realimagtocomplex setAddr]
template <typename T> void RealimagToComplex<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pIn0 = static_cast<T *>(addr.src[0]);
      _pIn1 = static_cast<T *>(addr.src[1]);
   }

   if (mode == kAddrModeOut) {

      _pOut = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pIn0 = static_cast<T *>(addr.src[0]);
      _pIn1 = static_cast<T *>(addr.src[1]);
      _pOut = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void RealimagToComplex<T>::exec() noexcept
{
   DSPLIB_realImagToComplex_exec(_handle.get(), _pIn0, _pIn1, _pOut);
}

template class RealimagToComplex<float>;
template class RealimagToComplex<double>;
template class RealimagToComplex<int16_t>;
template class RealimagToComplex<uint16_t>;
template class RealimagToComplex<int32_t>;
template class RealimagToComplex<uint32_t>;

} // namespace DSPLIB
} // namespace TISP
