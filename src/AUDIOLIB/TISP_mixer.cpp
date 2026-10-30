// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "DSPLIB_interleave/DSPLIB_interleave.h"
#include "DSPLIB_matMul/DSPLIB_matMul.h"
#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_mixer.hpp>
#include <cstddef>
#include <cstring>
#include <memory>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Mixer constructor]

template <typename T>
Mixer<T>::Mixer(TISP_BufDesc2D *pX,
                size_t          numInputs,
                T              *pMixerCoeff,
                size_t          widthCoeff,
                size_t          heightCoeff,
                size_t          strideCoeff,
                T              *pScratch,
                size_t          heightScratch,
                T              *pOut,
                size_t          widthOut,
                size_t          heightOut,
                size_t          strideOut,
                T              *pOutTemp,
                size_t          widthTempOut,
                size_t          heightTempOut,
                size_t          strideTempOut,
                size_t          isInterleave,
                ErrorCtxt      &errorCtx)
    : _numInputs(numInputs), _pX(pX), _pY(pOut), _pYTemp(pOutTemp), _pCoeff(pMixerCoeff), _pScratch(pScratch),
      _widthOut(widthOut), _heightOut(heightOut), _strideOut(strideOut), _widthOutTemp(widthTempOut),
      _heightOutTemp(heightTempOut), _strideOutTemp(strideTempOut), _widthCoeff(widthCoeff), _heightCoeff(heightCoeff),
      _strideCoeff(strideCoeff), _heightScratch(heightScratch), __isInterleave(isInterleave)
{
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numInputs=%zu, widthOut=%zu, heightOut=%zu",
                   numInputs, widthOut, heightOut);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Mixer constructor", "input pointer pX is null");
   }
   if (numInputs == 0) {
      errorCtx.setNullPointerError("Mixer constructor", "numInputs must be greater than zero");
   }
   if (pMixerCoeff == nullptr) {
      errorCtx.setNullPointerError("Mixer constructor", "mixing coefficient pointer pMixerCoeff is null");
   }
   if (widthCoeff == 0) {
      errorCtx.setNullPointerError("Mixer constructor", "widthCoeff must be greater than zero");
   }
   if (heightCoeff == 0) {
      errorCtx.setNullPointerError("Mixer constructor", "heightCoeff must be greater than zero");
   }
   if (pScratch == nullptr) {
      errorCtx.setNullPointerError("Mixer constructor", "scratch pointer pScratch is null");
   }
   if (heightScratch == 0) {
      errorCtx.setNullPointerError("Mixer constructor", "heightScratch must be greater than zero");
   }
   if (pOut == nullptr) {
      errorCtx.setNullPointerError("Mixer constructor", "output pointer pOut is null");
   }
   if (widthOut == 0) {
      errorCtx.setInvalidParameterError("Mixer constructor", "widthOut must be greater than zero");
   }
   if (heightOut == 0) {
      errorCtx.setInvalidParameterError("Mixer constructor", "heightOut must be greater than zero");
   }
   if (pOutTemp == nullptr) {
      errorCtx.setInvalidParameterError("Mixer constructor", "temporary output buffer pOutTemp is null");
   }
   if (widthTempOut == 0) {
      errorCtx.setInvalidParameterError("Mixer constructor", "widthTempOut must be greater than zero");
   }
   if (heightTempOut == 0) {
      errorCtx.setInvalidParameterError("Mixer constructor", "heightTempOut must be greater than zero");
   }
   if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("Mixer constructor", "isInterleave must be either zero or one");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

      // Initialize bufParams with node input and output buffer parameters. The
      // mixer has one geometry per input source (pX[i]); populate up to
      // kMaxSrcAddr slots (validation above guarantees pX is non-null here).
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      size_t numBufIn = (numInputs < static_cast<size_t>(kMaxSrcAddr)) ? numInputs : static_cast<size_t>(kMaxSrcAddr);
      for (size_t i = 0; i < numBufIn; i++) {
         _nodeInfo.bufParams.widthIn[i]      = pX[i].width;
         _nodeInfo.bufParams.heightIn[i]     = pX[i].height;
         _nodeInfo.bufParams.inPitchBytes[i] = pX[i].pitchBytes;
      }

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      _nodeInfo.bufParams.widthOut[0]        = widthOut;
      _nodeInfo.bufParams.heightOut[0]       = heightOut;
      _nodeInfo.bufParams.outPitchBytes[0]   = strideOut;

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_inputAggregator     */
      /***********************************************************************/
      AUDIOLIB_inputAggregator_InitArgs kerInitArgs;
      kerInitArgs.funcStyle          = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.numInputs          = numInputs;
      kerInitArgs.totalInChannels    = heightScratch;
      kerInitArgs.isInputInterleave  = isInterleave;
      kerInitArgs.isOutputInterleave = 0;

      _handle = make_unique_handle<char>(AUDIOLIB_inputAggregator_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Mixer constructor", "failed to allocate inputAggregator handle");
      }

      if (errorCtx.isSuccess()) {

         // Allocate the scratch pointer array ONCE here
         _inputPtrs = std::make_unique<void*[]>(numInputs); 

         if (!_inputPtrs) {
            errorCtx.setMemoryError("Mixer constructor", "failed to allocate input pointer array");
         }

         // Temp allocation for bufParams (only needed for initialization)
         auto                   bufParamsIn = std::make_unique<AUDIOLIB_bufParams2D_t[]>(numInputs);
         AUDIOLIB_bufParams2D_t bufParamsOut;

         if (errorCtx.isSuccess()) {
            for (uint32_t i = 0; i < numInputs; i++) {
               bufParamsIn[i].data_type = AUDIOLIB_element_dType<T>::value;
               bufParamsIn[i].dim_x     = static_cast<uint32_t>(pX[i].width);
               bufParamsIn[i].dim_y     = static_cast<uint32_t>(pX[i].height);
               bufParamsIn[i].stride_y  = static_cast<int32_t>(pX[i].pitchBytes);

               // Initialize the member pointer array to avoid nulls
               _inputPtrs[i] = static_cast<void *>(pX[i].pBuf);
            }
         }
         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(isInterleave ? widthTempOut : widthOut);
         bufParamsOut.dim_y     = static_cast<uint32_t>(heightScratch);
         bufParamsOut.stride_y  = static_cast<int32_t>(isInterleave ? strideTempOut : strideOut);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_inputAggregator_init_checkParams(_handle.get(), bufParamsIn.get(), &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_inputAggregator_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for inputAggregator to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet =
                AUDIOLIB_inputAggregator_init(_handle.get(), bufParamsIn.get(), &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_inputAggregator_init");
            if (errorCtx.isSuccess()) {

               /* ================================================================= */
               /* 2. Initialize DSPLIB_matMul                                      */
               /* ================================================================= */

               DSPLIB_matMul_InitArgs matMulArgs;
               matMulArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

               // Allocate Handle for MatMul (Assuming _handleMatMul is defined in .hpp)
               _handleMatMul = make_unique_handle<char>(DSPLIB_matMul_getHandleSize(&matMulArgs));

               if (!_handleMatMul) {
                  errorCtx.setMemoryError("Mixer constructor", "failed to allocate matMul handle");
               }

               if (errorCtx.isSuccess()) {

                  DSPLIB_bufParams2D_t mmIn0;
                  DSPLIB_bufParams2D_t mmIn1;
                  DSPLIB_bufParams2D_t mmOut;

                  // Matrix A: Coefficients [NumOutputs x TotalInChannels]
                  mmIn0.data_type = AUDIOLIB_element_dType<T>::value;
                  mmIn0.dim_x     = static_cast<uint32_t>(widthCoeff);  // K: Total In Channels
                  mmIn0.dim_y     = static_cast<uint32_t>(heightCoeff); // M: Num Output Channels
                  mmIn0.stride_y  = static_cast<int32_t>(strideCoeff);

                  // Matrix B: Input (Scratch) [TotalInChannels x Samples]
                  mmIn1.data_type = AUDIOLIB_element_dType<T>::value;
                  mmIn1.dim_x     = static_cast<uint32_t>(isInterleave ? widthTempOut : widthOut); // N: Samples
                  mmIn1.dim_y     = static_cast<uint32_t>(heightScratch); // K: Total In Channels
                  mmIn1.stride_y =
                      static_cast<int32_t>(isInterleave ? strideTempOut : strideOut); // Stride of the scratch buffer

                  // Matrix C: Output [NumOutputs x Samples]
                  mmOut.data_type = AUDIOLIB_element_dType<T>::value;
                  mmOut.dim_x     = static_cast<uint32_t>(isInterleave ? widthTempOut : widthOut); // N: Samples
                  mmOut.dim_y =
                      static_cast<uint32_t>(isInterleave ? heightTempOut : heightOut); // M: Num Output Channels
                  mmOut.stride_y = static_cast<int32_t>(isInterleave ? strideTempOut : strideOut); // Output stride

                  /******************************************************************************/
                  /* Validate parameters before init                                            */
                  /******************************************************************************/

                  DSPLIB_STATUS checkRet = DSPLIB_matMul_init_checkParams(
                      static_cast<DSPLIB_kernelHandle>(_handleMatMul.get()), &mmIn0, &mmIn1, &mmOut, &matMulArgs);
                  errorCtx.remapXLIBError(checkRet, "DSPLIB_matMul_init_checkParams");

                  /******************************************************************************/
                  /* Call init function from DSPLIB to for matMul to set SE/SA params             */
                  /******************************************************************************/

                  if (errorCtx.isSuccess()) {
                     DSPLIB_STATUS initRet = DSPLIB_matMul_init(static_cast<DSPLIB_kernelHandle>(_handleMatMul.get()),
                                                                &mmIn0, &mmIn1, &mmOut, &matMulArgs);

                     errorCtx.remapXLIBError(initRet, "DSPLIB_matMul_init");
                     if (errorCtx.isSuccess()) {

                        if (isInterleave) {
                           DSPLIB_interleaveInitArgs kerInitArgsI;
                           kerInitArgsI.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

                           _handleInterleave = make_unique_handle<char>(DSPLIB_interleave_getHandleSize(&kerInitArgsI));

                           if (!_handleInterleave) {
                              errorCtx.setMemoryError("Mixer constructor", "failed to allocate interleave handle");
                           }

                           if (errorCtx.isSuccess()) {

                              DSPLIB_bufParams2D_t bufParamsOutI;

                              bufParamsOutI.data_type = AUDIOLIB_element_dType<T>::value;
                              bufParamsOutI.dim_x     = static_cast<uint32_t>(widthOut);
                              bufParamsOutI.dim_y     = static_cast<uint32_t>(heightOut);
                              bufParamsOutI.stride_y  = static_cast<int32_t>(strideOut);

                              /******************************************************************************/
                              /* Validate parameters before init                                            */
                              /******************************************************************************/

                              DSPLIB_STATUS checkRetI = DSPLIB_interleave_init_checkParams(
                                  _handleInterleave.get(), &mmOut, &bufParamsOutI, &kerInitArgsI);
                              errorCtx.remapXLIBError(checkRetI, "DSPLIB_interleave_init_checkParams");

                              /******************************************************************************/
                              /* Call init function from DSPLIB to for interleave to set SE/SA params             */
                              /******************************************************************************/

                              if (errorCtx.isSuccess()) {
                                 DSPLIB_STATUS initRetI = DSPLIB_interleave_init(_handleInterleave.get(), &mmOut,
                                                                                 &bufParamsOutI, &kerInitArgsI);

                                 errorCtx.remapXLIBError(initRetI, "DSPLIB_interleave_init");
                              }
                           }
                        }

                        if (errorCtx.isSuccess()) {
                           _execImpl = (isInterleave == 1) ? &Mixer::_execInterleaved : &Mixer::_execNonInterleaved;
                        }
                     }
                  }
               }
            }
         }
      }
   }
}

template <typename T> void *Mixer<T>::getInputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   if (index < _numInputs) {
      result = static_cast<void *>(_pX[index].pBuf);
   }
   return result;
}

template <typename T> void *Mixer<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Mixer<T>::getNumInputs() const noexcept { return _numInputs; }

template <typename T> size_t Mixer<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Mixer setAddr]
template <typename T> void Mixer<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   uint32_t i = 0;
   if (mode == kAddrModeIn || mode == kAddrModeInOut) {
      for (i = 0; i < _numInputs; i++) {
         _pX[i].pBuf   = static_cast<T *>(addr.src[i]);
         _inputPtrs[i] = _pX[i].pBuf;
      }
   }
   if (mode == kAddrModeOut || mode == kAddrModeInOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void Mixer<T>::_execNonInterleaved() noexcept
{
   // 2. Perform Matrix Multiplication
   DSPLIB_matMul_exec(static_cast<DSPLIB_kernelHandle>(_handleMatMul.get()), static_cast<void *>(_pCoeff),
                      static_cast<void *>(_pScratch), static_cast<void *>(_pY));
}

template <typename T> void Mixer<T>::_execInterleaved() noexcept
{
   // 2. Perform Matrix Multiplication
   DSPLIB_matMul_exec(static_cast<DSPLIB_kernelHandle>(_handleMatMul.get()), static_cast<void *>(_pCoeff),
                      static_cast<void *>(_pScratch), static_cast<void *>(_pYTemp));
   // 3. Interleave the output
   DSPLIB_interleave_exec(static_cast<DSPLIB_kernelHandle>(_handleInterleave.get()), static_cast<void *>(_pYTemp),
                          static_cast<void *>(_pY));
}

template <typename T> void Mixer<T>::exec() noexcept
{
   // 1. Aggregate inputs
   AUDIOLIB_inputAggregator_exec(_handle.get(), _inputPtrs.get(), static_cast<void *>(_pScratch));

   (this->*_execImpl)();
}

//! [TISP::Mixer delegating constructor]
template <typename T>
Mixer<T>::Mixer(TISP_BufDesc2D *pX,
                size_t          numInputs,
                T              *pMixerCoeff,
                size_t          widthCoeff,
                size_t          heightCoeff,
                size_t          strideCoeff,
                T              *pScratch,
                size_t          heightScratch,
                T              *pOut,
                size_t          widthOut,
                size_t          heightOut,
                size_t          strideOut,
                T              *pOutTemp,
                size_t          widthOutTemp,
                size_t          heightTempOut,
                size_t          strideTempOut,
                size_t          isInterleave,
                const char     *nodeName,
                uint32_t        nodeId,
                ErrorCtxt      &errorCtx)
    : Mixer(pX,
            numInputs,
            pMixerCoeff,
            widthCoeff,
            heightCoeff,
            strideCoeff,
            pScratch,
            heightScratch,
            pOut,
            widthOut,
            heightOut,
            strideOut,
            pOutTemp,
            widthOutTemp,
            heightTempOut,
            strideTempOut,
            isInterleave,
            errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

//! [TISP::Mixer getNodeName]

//! [TISP::Mixer getNodeId]

//! [TISP::Mixer getParams]

template class Mixer<float>;
template class Mixer<double>;

} // namespace AUDIOLIB
} // namespace TISP
