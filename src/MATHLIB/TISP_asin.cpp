// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_asin.hpp"
#include "TISP_MATHLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <cstring>

namespace TISP {
namespace MATHLIB {

template <typename T>
Asin<T>::Asin(size_t size, T *pIn, T *pOut, ErrorCtxt &errorCtx) : _pIn(pIn), _pOut(pOut), _size(size)
{
   (void) size;
   (void) pIn;
   (void) pOut;

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "size=%zu", size);

   // Input validation checks
   if (pIn == nullptr) {
      errorCtx.setNullPointerError("Asin constructor", "Input pointer pIn is null");
   }
   if (pOut == nullptr) {
      errorCtx.setNullPointerError("Asin constructor", "Output pointer pOut is null");
   }
   if (size == 0) {
      errorCtx.setInvalidParameterError("Asin constructor", "Size must be greater than zero");
   }

   // all buffer params are initialised inside the function

   // Initialize bufParams with node input and output buffer parameters
   _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
   _nodeInfo.bufParams.isInputInterleave = 0;
   _nodeInfo.bufParams.widthIn[0]        = size;
   _nodeInfo.bufParams.heightIn[0]       = 1;
   _nodeInfo.bufParams.inPitchBytes[0]   = size * sizeof(T);

   _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
   _nodeInfo.bufParams.isOutputInterleave = 0;
   _nodeInfo.bufParams.widthOut[0]        = size;
   _nodeInfo.bufParams.heightOut[0]       = 1;
   _nodeInfo.bufParams.outPitchBytes[0]   = size * sizeof(T);
}

template <typename T> void *Asin<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pIn) : nullptr;
}

template <typename T> void *Asin<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pOut) : nullptr;
}

template <typename T> size_t Asin<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Asin<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Asin<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pIn = static_cast<T *>(addr.src[0]);
   }

   if (mode == kAddrModeOut) {

      _pOut = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pIn  = static_cast<T *>(addr.src[0]);
      _pOut = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void Asin<T>::exec() noexcept { MATHLIB_asin(_size, _pIn, _pOut); }

template <typename T>
Asin<T>::Asin(size_t size, T *pIn, T *pOut, const char *nodeName, uint32_t nodeId, ErrorCtxt &errorCtx)
    : Asin(size, pIn, pOut, errorCtx)
{
   (void) size;
   (void) pIn;
   (void) pOut;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Asin<float>;

} // namespace MATHLIB
} // namespace TISP
