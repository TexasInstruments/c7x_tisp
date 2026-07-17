// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_addConstant.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"

namespace TISP {
namespace DSPLIB {

template <typename T>
AddConstant<T>::AddConstant(T *pIn0, T *pIn1, T *pOut, size_t size, ErrorCtxt &errorCtx)
    : _pIn0(pIn0), _pIn1(pIn1), _pOut(pOut), _size(size)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "size=%zu", size);

   // Input validation checks
   if (pIn0 == nullptr) {
      errorCtx.setNullPointerError("AddConstant constructor", "constant pointer pIn0 is null");
   }
   if (pIn1 == nullptr) {
      errorCtx.setNullPointerError("AddConstant constructor", "input pointer pIn1 is null");
   }
   if (pOut == nullptr) {
      errorCtx.setNullPointerError("AddConstant constructor", "output pointer pOut is null");
   }
   if (size == 0) {
      errorCtx.setInvalidParameterError("AddConstant constructor", "size must be greater than zero");
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

      DSPLIB_bufParams1D_t        bufParamsIn;
      DSPLIB_bufParams1D_t        bufParamsOut;
      DSPLIB_addConstant_InitArgs kerInitArgs;

      kerInitArgs.dataSize  = size;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      bufParamsIn.dim_x  = static_cast<uint32_t>(size);
      bufParamsOut.dim_x = static_cast<uint32_t>(size);

      bufParamsIn.data_type  = DSPLIB_element_dType<T>::value;
      bufParamsOut.data_type = DSPLIB_element_dType<T>::value;

      _handle = make_unique_handle<char>(DSPLIB_addConstant_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("AddConstant constructor", "failed to allocate addConstant handle");
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_STATUS checkRet =
             DSPLIB_addConstant_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_AddConstant_init_checkParams");

         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS initRet = DSPLIB_addConstant_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "DSPLIB_AddConstant_init");
         }
      }
   }
}

template <typename T>
AddConstant<T>::AddConstant(T          *pIn0,
                            T          *pIn1,
                            T          *pOut,
                            size_t      size,
                            const char *nodeName,
                            uint32_t    nodeId,
                            ErrorCtxt  &errorCtx)
    : AddConstant(pIn0, pIn1, pOut, size, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *AddConstant<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pIn1) : nullptr;
}

template <typename T> void *AddConstant<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pOut) : nullptr;
}

template <typename T> size_t AddConstant<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t AddConstant<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void AddConstant<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pIn1 = static_cast<T *>(addr.src[0]);
   }

   if (mode == kAddrModeOut) {

      _pOut = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pIn1 = static_cast<T *>(addr.src[0]);
      _pOut = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void AddConstant<T>::exec() noexcept
{
   DSPLIB_addConstant_exec(_handle.get(), _pIn0, _pIn1, _pOut);
}

template class AddConstant<float>;

} // namespace DSPLIB
} // namespace TISP
