// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_div.hpp"
#include "TISP_MATHLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <cstring>

namespace TISP {
namespace MATHLIB {

template <typename T>
Div<T>::Div(size_t size, T *pIn0, T *pIn1, T *pOut, ErrorCtxt &errorCtx)
    : _pIn0(pIn0), _pIn1(pIn1), _pOut(pOut), _size(size)
{
   (void) size;
   (void) pIn0;
   (void) pIn1;
   (void) pOut;

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "size=%zu", size);

   // Input validation checks
   if (pIn0 == nullptr) {
      errorCtx.setNullPointerError("Div constructor", "Input pointer pIn0 is null");
   }
   if (pIn1 == nullptr) {
      errorCtx.setNullPointerError("Div constructor", "Input pointer pIn1 is null");
   }
   if (pOut == nullptr) {
      errorCtx.setNullPointerError("Div constructor", "Output pointer pOut is null");
   }
   if (size == 0) {
      errorCtx.setInvalidParameterError("Div constructor", "Size must be greater than zero");
   }

   // Initialize bufParams with node input and output buffer parameters
   _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
   _nodeInfo.bufParams.isInputInterleave = 0;
   _nodeInfo.bufParams.widthIn[0]        = size;
   _nodeInfo.bufParams.heightIn[0]       = 1;
   _nodeInfo.bufParams.inPitchBytes[0]   = size * sizeof(T);
   _nodeInfo.bufParams.widthIn[1]        = size;
   _nodeInfo.bufParams.heightIn[1]       = 1;
   _nodeInfo.bufParams.inPitchBytes[1]   = size * sizeof(T);

   _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
   _nodeInfo.bufParams.isOutputInterleave = 0;
   _nodeInfo.bufParams.widthOut[0]        = size;
   _nodeInfo.bufParams.heightOut[0]       = 1;
   _nodeInfo.bufParams.outPitchBytes[0]   = size * sizeof(T);
}

template <typename T> void *Div<T>::getInputAddr(size_t index) const noexcept
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

template <typename T> void *Div<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pOut) : nullptr;
}

template <typename T> size_t Div<T>::getNumInputs() const noexcept { return 2; }

template <typename T> size_t Div<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Div<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Div<T>::exec() noexcept { MATHLIB_div(_size, _pIn0, _pIn1, _pOut); }

template <typename T>
Div<T>::Div(size_t size, T *pIn0, T *pIn1, T *pOut, const char *nodeName, uint32_t nodeId, ErrorCtxt &errorCtx)
    : Div(size, pIn0, pIn1, pOut, errorCtx)
{
   (void) size;
   (void) pIn0;
   (void) pIn1;
   (void) pOut;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Div<float>;

} // namespace MATHLIB
} // namespace TISP
