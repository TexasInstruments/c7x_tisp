// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_balance.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Balance constructor]

template <typename T>
Balance<T>::Balance(T         *pXL,
                    T         *pXR,
                    T         *pYL,
                    T         *pYR,
                    size_t     widthIn,
                    size_t     heightIn,
                    size_t     inPitchBytes,
                    size_t     outPitchBytes,
                    float      balance,
                    float      smoothingTime,
                    int32_t    samplingRate,
                    ErrorCtxt &errorCtx)
    : _pXL(pXL), _pXR(pXR), _pYL(pYL), _pYR(pYR)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", widthIn, heightIn);

   // Input validation checks
   if (pXL == nullptr) {
      errorCtx.setNullPointerError("Balance constructor", "input pointer pXL is null");
   }
   if (pXR == nullptr) {
      errorCtx.setNullPointerError("Balance constructor", "input pointer pXR is null");
   }
   if (pYL == nullptr) {
      errorCtx.setNullPointerError("Balance constructor", "output pointer pYL is null");
   }
   if (pYR == nullptr) {
      errorCtx.setNullPointerError("Balance constructor", "output pointer pYR is null");
   }
   if (widthIn == 0) {
      errorCtx.setInvalidParameterError("Balance constructor", "width must be greater than zero");
   }
   if (heightIn == 0) {
      errorCtx.setInvalidParameterError("Balance constructor", "height must be greater than zero");
   }
   if (balance < -1 || balance > 1) {
      errorCtx.setInvalidParameterError("Balance constructor", "left/right control must be between -1 and 1");
   }
   if (smoothingTime < 0 || smoothingTime > 1000) {
      errorCtx.setInvalidParameterError("Balance constructor", "time constant must be between 0 and 1000");
   }
   if (samplingRate == 0) {
      errorCtx.setInvalidParameterError("Balance constructor", "samplingRate must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _widthIn       = widthIn;
      _heightIn      = heightIn;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _balance       = balance;
      _smoothingTime = smoothingTime;
      _samplingRate  = samplingRate;

      // Initialize bufParams with node input and output buffer parameters. The node has two
      // input buffers (pXL, pXR) and two output buffers (pYL, pYR) sharing the same geometry.
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
      _nodeInfo.bufParams.widthOut[0]        = widthIn;
      _nodeInfo.bufParams.heightOut[0]       = heightIn;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      _nodeInfo.bufParams.widthOut[1]        = widthIn;
      _nodeInfo.bufParams.heightOut[1]       = heightIn;
      _nodeInfo.bufParams.outPitchBytes[1]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Balance from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_balance_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;

      AUDIOLIB_balance_SetArgs kerSetArgs;
      kerSetArgs.balance       = balance;
      kerSetArgs.smoothingTime = smoothingTime;
      kerSetArgs.samplingRate  = samplingRate;

      _handle = make_unique_handle<char>(AUDIOLIB_balance_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Balance constructor", "failed to allocate balance handle");
      }

      if (errorCtx.isSuccess()) {

         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(widthIn);
         bufParamsIn.dim_y     = static_cast<uint32_t>(heightIn);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(widthIn);
         bufParamsOut.dim_y     = static_cast<uint32_t>(heightIn);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_balance_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_balance_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for balance to set SE/SA params   */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_balance_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_balance_init");
            if (errorCtx.isSuccess()) {
               AUDIOLIB_STATUS setRet = AUDIOLIB_balance_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(setRet, "AUDIOLIB_balance_set");
            }
         }
      }
   }
}

template <typename T> void *Balance<T>::getInputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   switch (index) {
   case 0:
      result = static_cast<void *>(_pXL);
      break;
   case 1:
      result = static_cast<void *>(_pXR);
      break;
   default:
      result = nullptr;
      break;
   }
   return result;
}

template <typename T> void *Balance<T>::getOutputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   switch (index) {
   case 0:
      result = static_cast<void *>(_pYL);
      break;
   case 1:
      result = static_cast<void *>(_pYR);
      break;
   default:
      result = nullptr;
      break;
   }
   return result;
}

template <typename T> size_t Balance<T>::getNumInputs() const noexcept { return 2; }

template <typename T> size_t Balance<T>::getNumOutputs() const noexcept { return 2; }

//! [TISP::Balance setAddr]
template <typename T> void Balance<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pXL = static_cast<T *>(addr.src[0]);
      _pXR = static_cast<T *>(addr.src[1]);
   }
   if (mode == kAddrModeOut) {
      _pYL = static_cast<T *>(addr.dst[0]);
      _pYR = static_cast<T *>(addr.dst[1]);
   }

   if (mode == kAddrModeInOut) {
      _pXL = static_cast<T *>(addr.src[0]);
      _pXR = static_cast<T *>(addr.src[1]);
      _pYL = static_cast<T *>(addr.dst[0]);
      _pYR = static_cast<T *>(addr.dst[1]);
   }
}

template <typename T> void Balance<T>::exec() noexcept
{
   AUDIOLIB_balance_exec(_handle.get(), static_cast<void *>(_pXL), static_cast<void *>(_pXR), static_cast<void *>(_pYL),
                         static_cast<void *>(_pYR));
}

template <typename T>
Balance<T>::Balance(T          *pXL,
                    T          *pXR,
                    T          *pYL,
                    T          *pYR,
                    size_t      widthIn,
                    size_t      heightIn,
                    size_t      inPitchBytes,
                    size_t      outPitchBytes,
                    float       balance,
                    float       smoothingTime,
                    int32_t     samplingRate,
                    const char *nodeName,
                    uint32_t    nodeId,
                    ErrorCtxt  &errorCtx)
    : Balance(pXL,
              pXR,
              pYL,
              pYR,
              widthIn,
              heightIn,
              inPitchBytes,
              outPitchBytes,
              balance,
              smoothingTime,
              samplingRate,
              errorCtx)
{
   (void) pXL;
   (void) pXR;
   (void) pYL;
   (void) pYR;
   (void) widthIn;
   (void) heightIn;
   (void) inPitchBytes;
   (void) outPitchBytes;
   (void) balance;
   (void) smoothingTime;
   (void) samplingRate;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Balance<float>;

} // namespace AUDIOLIB
} // namespace TISP
