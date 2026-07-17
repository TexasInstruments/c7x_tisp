// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include <TISP_matMul.hpp>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [DSPLIB_matMul constructor]

template <typename T>
MatMul<T>::MatMul(T         *_pA,
                  T         *_pB,
                  T         *_pY,
                  size_t     _widthA,
                  size_t     _heightA,
                  size_t     _widthB,
                  size_t     _heightB,
                  size_t     _inPitchA,
                  size_t     _inPitchB,
                  size_t     _outPitch,
                  ErrorCtxt &errorCtx)
    : pA(_pA), pB(_pB), pY(_pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "widthA=%zu,heightA=%zu,widthB=%zu", _widthA,
                   _heightA, _widthB);

   // Input validation checks
   if (pA == nullptr) {
      errorCtx.setNullPointerError("MatMul constructor", "input pointer pX is null");
   }
   if (pB == nullptr) {
      errorCtx.setNullPointerError("MatMul constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("MatMul constructor", "output pointer pY is null");
   }
   if (_widthA == 0) {
      errorCtx.setInvalidParameterError("MatMul constructor", "widthA must be greater than zero");
   }
   if (_heightA == 0) {
      errorCtx.setInvalidParameterError("MatMul constructor", "heightA must be greater than zero");
   }
   if (_widthB == 0) {
      errorCtx.setInvalidParameterError("MatMul constructor", "widthB must be greater than zero");
   }
   if (_heightB == 0) {
      errorCtx.setInvalidParameterError("MatMul constructor", "heightB must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

      // Initialize bufParams with node input and output buffer parameters. MatMul
      // has two distinct matrix operands (A and B); populate one slot per operand.
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = _widthA;
      _nodeInfo.bufParams.heightIn[0]       = _heightA;
      _nodeInfo.bufParams.inPitchBytes[0]   = _inPitchA;
      _nodeInfo.bufParams.widthIn[1]        = _widthB;
      _nodeInfo.bufParams.heightIn[1]       = _heightB;
      _nodeInfo.bufParams.inPitchBytes[1]   = _inPitchB;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = _widthB;
      _nodeInfo.bufParams.heightOut[0]       = _heightA;
      _nodeInfo.bufParams.outPitchBytes[0]   = _outPitch;
      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_matMul from DSPLIB */
      /***********************************************************************/
      DSPLIB_matMul_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      handle = std::make_unique<char[]>(DSPLIB_matMul_getHandleSize(&kerInitArgs));

      if (!handle) {
         errorCtx.setMemoryError("MatMul constructor", "failed to allocate matMul handle");
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_bufParams2D_t bufParamsInA;
         DSPLIB_bufParams2D_t bufParamsInB;
         DSPLIB_bufParams2D_t bufParamsOut;

         bufParamsInA.data_type = DSPLIB_element_dType<T>::value;
         bufParamsInA.dim_x     = static_cast<uint32_t>(_widthA);
         bufParamsInA.dim_y     = static_cast<uint32_t>(_heightA);
         bufParamsInA.stride_y  = static_cast<int32_t>(_inPitchA);

         bufParamsInB.data_type = DSPLIB_element_dType<T>::value;
         bufParamsInB.dim_x     = static_cast<uint32_t>(_widthB);
         bufParamsInB.dim_y     = static_cast<uint32_t>(_heightB);
         bufParamsInB.stride_y  = static_cast<int32_t>(_inPitchB);

         bufParamsOut.data_type = DSPLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(_widthB);
         bufParamsOut.dim_y     = static_cast<uint32_t>(_heightA);
         bufParamsOut.stride_y  = static_cast<int32_t>(_outPitch);

         /***********************************************************************************/
         /* Call init function from DSPLIB to for matrix multiplication to set SE/SA params */
         /***********************************************************************************/

         DSPLIB_STATUS checkRet =
             DSPLIB_matMul_init_checkParams(handle.get(), &bufParamsInA, &bufParamsInB, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_matMul_init_checkParams");

         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS retVal = DSPLIB_matMul_exec_checkParams(handle.get(), &pA, &pB, &pY);
            errorCtx.remapXLIBError(retVal, "DSPLIB_matMul_exec_checkParams");
            if (errorCtx.isSuccess()) {
               DSPLIB_STATUS initRet =
                   DSPLIB_matMul_init(handle.get(), &bufParamsInA, &bufParamsInB, &bufParamsOut, &kerInitArgs);
               errorCtx.remapXLIBError(initRet, "DSPLIB_matMul_init");
            }
         }
      }
   }
}

//! [DSPLIB_matMul constructor]

template <typename T> void *MatMul<T>::getInputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   switch (index) {
   case 0:
      result = static_cast<void *>(pA);
      break;
   case 1:
      result = static_cast<void *>(pB);
      break;
   default:
      result = nullptr;
      break;
   }
   return result;
}

template <typename T> void *MatMul<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(pY) : nullptr;
}

template <typename T> size_t MatMul<T>::getNumInputs() const noexcept { return 2; }

template <typename T> size_t MatMul<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void MatMul<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      pA = static_cast<T *>(addr.src[0]);
      pB = static_cast<T *>(addr.src[1]);
   }
   if (mode == kAddrModeOut) {
      pY = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      pA = static_cast<T *>(addr.src[0]);
      pB = static_cast<T *>(addr.src[1]);
      pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void MatMul<T>::exec() noexcept { DSPLIB_matMul_exec(handle.get(), pA, pB, pY); }

template <typename T>
MatMul<T>::MatMul(T          *_pA,
                  T          *_pB,
                  T          *_pY,
                  size_t      _widthA,
                  size_t      _heightA,
                  size_t      _widthB,
                  size_t      _heightB,
                  size_t      _inPitchA,
                  size_t      _inPitchB,
                  size_t      _outPitch,
                  const char *nodeName,
                  uint32_t    nodeId,
                  ErrorCtxt  &errorCtx)
    : MatMul(_pA, _pB, _pY, _widthA, _heightA, _widthB, _heightB, _inPitchA, _inPitchB, _outPitch, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class MatMul<float>;
template class MatMul<double>;

} // namespace DSPLIB
} // namespace TISP
