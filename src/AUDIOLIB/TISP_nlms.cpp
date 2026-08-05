// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include "common/AUDIOLIB_types.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <DSPLIB/TISP_DSPLIB_types.hpp>
#include <TISP_nlms.hpp>
#include <cstdint>
#include <cstring>
#include <random>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Nlms constructor]

template <typename T>
Nlms<T>::Nlms(T         *pIn,
              T         *pInDesired,
              T         *pStateBuffer,
              T         *pScratchBuffer,
              T         *pCoefficients,
              T         *pOut,
              T         *pInD,
              T         *pInDesiredD,
              T         *pOutD,
              size_t     widthIn,
              size_t     heightIn,
              size_t     filterLength,
              float      stepSize,
              size_t     numChannels,
              size_t     inPitchBytes,
              size_t     outPitchBytes,
              size_t     isInterleave,
              size_t     historyBufSize,
              ErrorCtxt &errorCtx)
    : _pIn(pIn), _pInDesired(pInDesired), _pStateBuffer(pStateBuffer), _pScratchBuffer(pScratchBuffer),
      _pCoefficients(pCoefficients), _pOut(pOut), _pInD(pInD), _pInDesiredD(pInDesiredD), _pOutI(pOutD)
{

   // Initialize node metadata
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "widthIn=%zu, heightIn=%zu, filterLength=%zu, stepSize=%.4f, numChannels=%zu", widthIn, heightIn,
                   filterLength, stepSize, numChannels);

   // Input validation checks
   if (pIn == nullptr) {
      errorCtx.setNullPointerError("Nlms constructor", "input pointer pIn is null");
   }
   if (pInDesired == nullptr) {
      errorCtx.setNullPointerError("Nlms constructor", "desired signal pointer pInDesired is null");
   }
   if (pStateBuffer == nullptr) {
      errorCtx.setNullPointerError("Nlms constructor", "circular state pointer pStateBuffer is null");
   }
   if (pScratchBuffer == nullptr) {
      errorCtx.setNullPointerError("Nlms constructor", "accumulator pointer pScratchBuffer is null");
   }
   if (pCoefficients == nullptr) {
      errorCtx.setNullPointerError("Nlms constructor", "coefficients pointer pCoefficients is null");
   }
   if (pOut == nullptr) {
      errorCtx.setNullPointerError("Nlms constructor", "output pointer pOut is null");
   }
   if (isInterleave == 1) {
      if (pInD == nullptr) {
         errorCtx.setNullPointerError("Nlms constructor", "deinterleave input pointer pInD is null");
      }
      if (pInDesiredD == nullptr) {
         errorCtx.setNullPointerError("Nlms constructor", "deinterleave desired signal pointer pIpInDesiredDn is null");
      }
      if (pOutD == nullptr) {
         errorCtx.setNullPointerError("Nlms constructor", "deinerleave output pointer pOutD is null");
      }
   }
   if (widthIn == 0) {
      errorCtx.setInvalidParameterError("Nlms constructor", "width must be greater than zero");
   }
   if (heightIn == 0) {
      errorCtx.setInvalidParameterError("Nlms constructor", "height must be greater than zero");
   }
   if (filterLength == 0) {
      errorCtx.setInvalidParameterError("Nlms constructor", "filterLength must be greater than zero");
   }
   if (stepSize == 0) {
      errorCtx.setInvalidParameterError("Nlms constructor", "stepSize must be greater than zero");
   }
   if (numChannels == 0) {
      errorCtx.setInvalidParameterError("Nlms constructor", "numChannels must be greater than zero");
   }
   if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("Nlms constructor", "isInterleave must be either zero or one");
   }
   if (historyBufSize == 0) {
      errorCtx.setInvalidParameterError("Nlms constructor", "historyBufSize must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _widthIn       = widthIn;
      _heightIn      = heightIn;
      _filterLength  = filterLength;
      _stepSize      = stepSize;
      _numChannels   = numChannels;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _isInterleave  = isInterleave;

      _historyBufSize = historyBufSize;

      // Initialize bufParams with node input and output buffer parameters. Nlms has two
      // inputs (pIn, pInDesired) of identical geometry and one output (pOut).
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      _nodeInfo.bufParams.widthIn[0]        = widthIn;
      _nodeInfo.bufParams.heightIn[0]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;
      _nodeInfo.bufParams.widthIn[1]        = widthIn;
      _nodeInfo.bufParams.heightIn[1]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[1]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      _nodeInfo.bufParams.widthOut[0]        = widthIn;
      _nodeInfo.bufParams.heightOut[0]       = heightIn;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Nlms from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_nlms_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.filterLength = filterLength;
      kerInitArgs.stepSize     = stepSize;
      kerInitArgs.numChannels  = numChannels;

      _handle = make_unique_handle<char>(AUDIOLIB_nlms_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Nlms constructor", "failed to allocate nlms handle");
      }

      if (errorCtx.isSuccess()) {

         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;

         if (isInterleave == 0) {
            bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsIn.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsIn.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

            bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsOut.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsOut.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
         }
         else {
            bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsIn.dim_x     = static_cast<uint32_t>(heightIn);
            bufParamsIn.dim_y     = static_cast<uint32_t>(widthIn);
            bufParamsIn.stride_y  = static_cast<int32_t>(outPitchBytes);

            bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsOut.dim_x     = static_cast<uint32_t>(heightIn);
            bufParamsOut.dim_y     = static_cast<uint32_t>(widthIn);
            bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);

            DSPLIB_deinterleaveInitArgs kerInitArgsD;
            kerInitArgsD.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

            _handle1 = make_unique_handle<char>(DSPLIB_deinterleave_getHandleSize(&kerInitArgsD));

            if (!_handle1) {
               errorCtx.setMemoryError("Nlms constructor", "failed to allocate deinterleave handle");
            }

            if (errorCtx.isSuccess()) {

               DSPLIB_bufParams2D_t bufParamsInD;
               DSPLIB_bufParams2D_t bufParamsOutD;

               bufParamsInD.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
               bufParamsInD.dim_x     = static_cast<uint32_t>(widthIn);
               bufParamsInD.dim_y     = static_cast<uint32_t>(heightIn);
               bufParamsInD.stride_y  = static_cast<int32_t>(inPitchBytes);

               bufParamsOutD.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
               bufParamsOutD.dim_x     = static_cast<uint32_t>(heightIn);
               bufParamsOutD.dim_y     = static_cast<uint32_t>(widthIn);
               bufParamsOutD.stride_y  = static_cast<int32_t>(outPitchBytes);

               /******************************************************************************/
               /* Validate parameters before init                                            */
               /******************************************************************************/

               DSPLIB_STATUS checkRet =
                   DSPLIB_deinterleave_init_checkParams(_handle1.get(), &bufParamsInD, &bufParamsOutD, &kerInitArgsD);
               errorCtx.remapXLIBError(checkRet, "DSPLIB_deinterleave_init_checkParams");
               /******************************************************************************/
               /* Call init function from DSPLIB to for deinterleave to set SE/SA params             */
               /******************************************************************************/

               if (errorCtx.isSuccess()) {
                  DSPLIB_STATUS initRet =
                      DSPLIB_deinterleave_init(_handle1.get(), &bufParamsInD, &bufParamsOutD, &kerInitArgsD);
                  errorCtx.remapXLIBError(initRet, "DSPLIB_deinterleave_init");
                  if (errorCtx.isSuccess()) {

                     DSPLIB_interleaveInitArgs kerInitArgsI;
                     kerInitArgsI.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

                     _handle2 = make_unique_handle<char>(DSPLIB_interleave_getHandleSize(&kerInitArgsI));

                     if (!_handle2) {
                        errorCtx.setMemoryError("Nlms constructor", "failed to allocate interleave handle");
                     }

                     if (errorCtx.isSuccess()) {

                        DSPLIB_bufParams2D_t bufParamsInI;
                        DSPLIB_bufParams2D_t bufParamsOutI;

                        bufParamsInI.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
                        bufParamsInI.dim_x     = static_cast<uint32_t>(heightIn);
                        bufParamsInI.dim_y     = static_cast<uint32_t>(widthIn);
                        bufParamsInI.stride_y  = static_cast<int32_t>(outPitchBytes);

                        bufParamsOutI.data_type = DSPLIB::DSPLIB_element_dType<T>::value;
                        bufParamsOutI.dim_x     = static_cast<uint32_t>(widthIn);
                        bufParamsOutI.dim_y     = static_cast<uint32_t>(heightIn);
                        bufParamsOutI.stride_y  = static_cast<int32_t>(outPitchBytes);

                        /******************************************************************************/
                        /* Validate parameters before init                                            */
                        /******************************************************************************/

                        DSPLIB_STATUS checkRet = DSPLIB_interleave_init_checkParams(_handle2.get(), &bufParamsInI,
                                                                                    &bufParamsOutI, &kerInitArgsI);
                        errorCtx.remapXLIBError(checkRet, "DSPLIB_interleave_init_checkParams");
                        /******************************************************************************/
                        /* Call init function from DSPLIB to for interleave to set SE/SA params             */
                        /******************************************************************************/

                        if (errorCtx.isSuccess()) {
                           DSPLIB_STATUS initRet =
                               DSPLIB_interleave_init(_handle2.get(), &bufParamsInI, &bufParamsOutI, &kerInitArgsI);
                           errorCtx.remapXLIBError(initRet, "DSPLIB_interleave_init");
                        }
                     }
                  }
               }
            }
         }

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_nlms_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_nlms_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for nlms to set SE/SA params           */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_nlms_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_nlms_init");
            if (errorCtx.isSuccess()) {
               cleanInternalBuffers();
               _execImpl = (isInterleave == 1) ? &Nlms::_execInterleaved : &Nlms::_execNonInterleaved;
            }
         }
      }
   }
}

template <typename T> void *Nlms<T>::getInputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   switch (index) {
   case 0:
      result = static_cast<void *>(_pIn);
      break;
   case 1:
      result = static_cast<void *>(_pInDesired);
      break;
   default:
      result = nullptr;
      break;
   }
   return result;
}

template <typename T> void *Nlms<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pOut) : nullptr;
}

template <typename T> size_t Nlms<T>::getNumInputs() const noexcept { return 2; }

template <typename T> size_t Nlms<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Nlms setAddr]
template <typename T> void Nlms<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pIn        = static_cast<T *>(addr.src[0]);
      _pInDesired = static_cast<T *>(addr.src[1]);
   }
   if (mode == kAddrModeOut) {
      _pOut = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pIn        = static_cast<T *>(addr.src[0]);
      _pInDesired = static_cast<T *>(addr.src[1]);
      _pOut       = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void Nlms<T>::_execNonInterleaved() noexcept
{
   AUDIOLIB_nlms_exec(_handle.get(), _pIn, _pInDesired, _pStateBuffer, _pScratchBuffer, _pCoefficients, _pOut);
}

template <typename T> void Nlms<T>::_execInterleaved() noexcept
{
   DSPLIB_deinterleave_exec(_handle1.get(), _pIn, _pInD);
   DSPLIB_deinterleave_exec(_handle1.get(), _pInDesired, _pInDesiredD);
   AUDIOLIB_nlms_exec(_handle.get(), _pInD, _pInDesiredD, _pStateBuffer, _pScratchBuffer, _pCoefficients, _pOutI);
   DSPLIB_interleave_exec(_handle2.get(), _pOutI, _pOut);
}

template <typename T> void Nlms<T>::exec() noexcept { (this->*_execImpl)(); }

//! [TISP::Nlms delegating constructor]
template <typename T>
Nlms<T>::Nlms(T          *pIn,
              T          *pInDesired,
              T          *pStateBuffer,
              T          *pScratchBuffer,
              T          *pCoefficients,
              T          *pOut,
              T          *pInD,
              T          *pInDesiredD,
              T          *pOutD,
              size_t      widthIn,
              size_t      heightIn,
              size_t      filterLength,
              float       stepSize,
              size_t      numChannels,
              size_t      inPitchBytes,
              size_t      outPitchBytes,
              size_t      isInterleave,
              size_t      historyBufSize,
              const char *nodeName,
              uint32_t    nodeId,
              ErrorCtxt  &errorCtx)
    : Nlms(pIn,
           pInDesired,
           pStateBuffer,
           pScratchBuffer,
           pCoefficients,
           pOut,
           pInD,
           pInDesiredD,
           pOutD,
           widthIn,
           heightIn,
           filterLength,
           stepSize,
           numChannels,
           inPitchBytes,
           outPitchBytes,
           isInterleave,
           historyBufSize,
           errorCtx)
{
   (void) pIn;
   (void) pInDesired;
   (void) pStateBuffer;
   (void) pScratchBuffer;
   (void) pCoefficients;
   (void) pOut;
   (void) pInD;
   (void) pInDesiredD;
   (void) pOutD;
   (void) widthIn;
   (void) heightIn;
   (void) filterLength;
   (void) stepSize;
   (void) numChannels;
   (void) inPitchBytes;
   (void) outPitchBytes;
   (void) isInterleave;
   (void) historyBufSize;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

//! [TISP::Nlms getNodeName]

//! [TISP::Nlms getNodeId]

//! [TISP::Nlms getParams]

template <typename T> void Nlms<T>::cleanInternalBuffers() noexcept
{
   if (_pStateBuffer != nullptr && _historyBufSize > 0) {
      std::memset(_pStateBuffer, 0, _historyBufSize);
   }
}

template class Nlms<float>;

} // namespace AUDIOLIB
} // namespace TISP
