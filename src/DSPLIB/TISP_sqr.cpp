// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_sqr.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_common.hpp"
#include "TISP_types.hpp"

namespace TISP {
namespace DSPLIB {

template <typename T> Sqr<T>::Sqr(T *pX, T *pY, size_t size, ErrorCtxt &errorCtx) : _pX(pX), _pY(pY), _size(size)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "size=%zu", size);
   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Sqr constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Sqr constructor", "output pointer pY is null");
   }
   if (size == 0) {
      errorCtx.setInvalidParameterError("Sqr constructor", "size must be greater than zero");
   }
   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

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

      DSPLIB_bufParams1D_t bufParamsIn;
      DSPLIB_bufParams1D_t bufParamsOut;
      DSPLIB_sqr_InitArgs  kerInitArgs;

      kerInitArgs.dataSize  = size;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      bufParamsIn.dim_x  = static_cast<uint32_t>(size);
      bufParamsOut.dim_x = static_cast<uint32_t>(size);

      bufParamsIn.data_type  = DSPLIB_element_dType<T>::value;
      bufParamsOut.data_type = DSPLIB_element_dType<T>::value;

      _handle = make_unique_handle<char>(DSPLIB_sqr_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Sqr constructor", "failed to allocate Sqr handle");
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_STATUS checkRet = DSPLIB_sqr_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_sqr_init_checkParams");

         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS initRet = DSPLIB_sqr_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "DSPLIB_sqr_init");
         }
      }
   }
}

template <typename T>
Sqr<T>::Sqr(T *pX, T *pY, size_t size, const char *nodeName, uint32_t nodeId, ErrorCtxt &errorCtx)
    : Sqr(pX, pY, size, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Sqr<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Sqr<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Sqr<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Sqr<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Sqr<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<T *>(addrs.src[0]);
   }

   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addrs.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<T *>(addrs.src[0]);
      _pY = static_cast<T *>(addrs.dst[0]);
   }
}

template <typename T> void Sqr<T>::exec() noexcept { DSPLIB_sqr_exec(_handle.get(), _pX, _pY); }

template class Sqr<float>;

} // namespace DSPLIB
} // namespace TISP
