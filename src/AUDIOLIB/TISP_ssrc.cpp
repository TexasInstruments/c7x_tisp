// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_ssrc.hpp>

namespace TISP {
namespace AUDIOLIB {

/* ssrc constructor */

template <typename T>
Ssrc<T>::Ssrc(T         *pX,
              T         *pState,
              T         *pFiltCoeffs,
              T         *pY,
              size_t     inputSampleCount,
              size_t     sampleDataType,
              size_t     widthIn,
              size_t     heightIn,
              size_t     inPitchBytes,
              size_t     widthOut,
              size_t     heightOut,
              size_t     outPitchBytes,
              size_t     inputSampleRate,
              size_t     outputSampleRate,
              size_t     numChannels,
              size_t     isInterleave,
              size_t     bufferFormat,
              size_t     enableMMA,
              size_t     mmaSize,
              ErrorCtxt &errorCtx)
    : _pX(pX), _pState(pState), _pFiltCoeffs(pFiltCoeffs), _pY(pY)
{
   /* Initialize _params with the node parameters */
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "widthIn=%zu,heightIn=%zu,widthOut=%zu,heightOut=%zu,isInterleave=%zu", widthIn, heightIn, widthOut,
                   heightOut, isInterleave);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Ssrc constructor", "input pointer pX is null");
   }
   if (pState == nullptr && bufferFormat == 0) {
      errorCtx.setNullPointerError("Ssrc constructor", "state pointer pState is null");
   }
   if (pFiltCoeffs == nullptr) {
      errorCtx.setNullPointerError("Ssrc constructor", "destination buffer pointer pFiltCoeffs is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Ssrc constructor", "output pointer pY is null");
   }
   if (inputSampleCount == 0) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "inputSampleCount must be greater than zero");
   }
   if (widthIn == 0) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "widthIn must be greater than zero");
   }
   if (heightIn == 0) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "heightIn must be greater than zero");
   }
   if (widthOut == 0) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "widthOut must be greater than zero");
   }
   if (heightOut == 0) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "heightOut must be greater than zero");
   }
   if (inputSampleRate > 15) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "inputSampleRate must be less than 192000(15)");
   }
   if (outputSampleRate > 15) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "statisticsType must be less than 192000(15)");
   }
   if (numChannels == 0) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "numChannels must be greater than zero");
   }
   if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "isInterleave must be either zero or one");
   }
   if (bufferFormat > 1) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "bufferFormat must be either zero or one");
   }
   if (enableMMA > 1) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "enableMMA must be either zero or one");
   }
   if (mmaSize == 0) {
      errorCtx.setInvalidParameterError("Ssrc constructor", "mmaSize must be greater than zero");
   }
   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _deinterleaveHandle = nullptr;
      _interleaveHandle   = nullptr;
      _blkCopy2DHandle1   = nullptr;
      _blkCopy2DHandle2   = nullptr;
      _blkCopy2DHandle3   = nullptr;
      _firHandle1         = nullptr;
      _firHandle2         = nullptr;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      _nodeInfo.bufParams.widthIn[0]        = widthIn;
      _nodeInfo.bufParams.heightIn[0]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      _nodeInfo.bufParams.widthOut[0]        = widthOut;
      _nodeInfo.bufParams.heightOut[0]       = heightOut;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_ssrc from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_ssrc_InitArgs kerInitArgs;
      kerInitArgs.funcStyle        = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.inputSampleCount = inputSampleCount;
      kerInitArgs.sampleDataType   = sampleDataType;
      kerInitArgs.inputSampleRate  = static_cast<ssrc_sample_rate_t>(inputSampleRate);
      kerInitArgs.outputSampleRate = static_cast<ssrc_sample_rate_t>(outputSampleRate);
      kerInitArgs.numChannels      = numChannels;
      kerInitArgs.dataFormat       = isInterleave;
      kerInitArgs.bufferFormat     = static_cast<AUDIOLIB_ssrc_buffer_format_t>(bufferFormat);
      kerInitArgs.enableMMA        = enableMMA;
      kerInitArgs.mmaSize          = mmaSize;

      if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
         if (bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_LINEAR) {

            _deinterleaveHandle = make_unique_handle<char>(DSPLIB_deinterleave_getHandleSize(nullptr));
            if (!_deinterleaveHandle) {
               errorCtx.setMemoryError("Ssrc constructor", "failed to allocate deinterleave handle");
            }

            if (errorCtx.isSuccess()) {
               _interleaveHandle = make_unique_handle<char>(DSPLIB_interleave_getHandleSize(nullptr));
               if (!_interleaveHandle) {
                  errorCtx.setMemoryError("Ssrc constructor", "failed to allocate interleave handle");
               }

               if (errorCtx.isSuccess()) {
                  _blkCopy2DHandle1 = make_unique_handle<char>(DSPLIB_blkCopy2D_getHandleSize(nullptr));
                  if (!_blkCopy2DHandle1) {
                     errorCtx.setMemoryError("Ssrc constructor", "failed to allocate block copy 2D handle");
                  }

                  if (errorCtx.isSuccess()) {
                     _blkCopy2DHandle2 = make_unique_handle<char>(DSPLIB_blkCopy2D_getHandleSize(nullptr));
                     if (!_blkCopy2DHandle2) {
                        errorCtx.setMemoryError("Ssrc constructor", "failed to allocate block copy 2D handle");
                     }

                     if (errorCtx.isSuccess()) {
                        _firHandle1 = make_unique_handle<char>(DSPLIB_fir_getHandleSize(nullptr));
                        if (!_firHandle1) {
                           errorCtx.setMemoryError("Ssrc constructor", "failed to allocate fir handle");
                        }
                        if (errorCtx.isSuccess()) {
                           _firHandle2 = make_unique_handle<char>(DSPLIB_fir_getHandleSize(nullptr));
                           if (!_firHandle2) {
                              errorCtx.setMemoryError("Ssrc constructor", "failed to allocate fir handle");
                           }
                        }
                     }
                  }
               }
            }
         }
      }
      else {
         _blkCopy2DHandle1 = make_unique_handle<char>(DSPLIB_blkCopy2D_getHandleSize(nullptr));
         if (!_blkCopy2DHandle1) {
            errorCtx.setMemoryError("Ssrc constructor", "failed to allocate block copy 2D handle");
         }

         if (errorCtx.isSuccess()) {
            _blkCopy2DHandle2 = make_unique_handle<char>(DSPLIB_blkCopy2D_getHandleSize(nullptr));
            if (!_blkCopy2DHandle2) {
               errorCtx.setMemoryError("Ssrc constructor", "failed to allocate block copy 2D handle");
            }

            if (errorCtx.isSuccess()) {
               _blkCopy2DHandle3 = make_unique_handle<char>(DSPLIB_blkCopy2D_getHandleSize(nullptr));
               if (!_blkCopy2DHandle3) {
                  errorCtx.setMemoryError("Ssrc constructor", "failed to allocate block copy 2D handle");
               }

               if (errorCtx.isSuccess()) {
                  _firHandle1 = make_unique_handle<char>(DSPLIB_fir_getHandleSize(nullptr));
                  if (!_firHandle1) {
                     errorCtx.setMemoryError("Ssrc constructor", "failed to allocate fir handle");
                  }
                  if (errorCtx.isSuccess()) {
                     _firHandle2 = make_unique_handle<char>(DSPLIB_fir_getHandleSize(nullptr));
                     if (!_firHandle2) {
                        errorCtx.setMemoryError("Ssrc constructor", "failed to allocate fir handle");
                     }
                  }
               }
            }
         }
      }

      if (errorCtx.isSuccess()) {
         kerInitArgs.deinterleaveHandle = _deinterleaveHandle.get();
         kerInitArgs.interleaveHandle   = _interleaveHandle.get();
         kerInitArgs.blkCopy2DHandle1   = _blkCopy2DHandle1.get();
         kerInitArgs.blkCopy2DHandle2   = _blkCopy2DHandle2.get();
         kerInitArgs.blkCopy2DHandle3   = _blkCopy2DHandle3.get();
         kerInitArgs.firHandle1         = _firHandle1.get();
         kerInitArgs.firHandle2         = _firHandle2.get();

         _handle = make_unique_handle<char>(AUDIOLIB_ssrc_getHandleSize(&kerInitArgs));
         if (!_handle) {
            errorCtx.setMemoryError("Ssrc constructor", "failed to allocate ssrc handle");
         }

         if (errorCtx.isSuccess()) {

            AUDIOLIB_bufParams2D_t bufParamsIn;
            AUDIOLIB_bufParams2D_t bufParamsOut;

            bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsIn.dim_x     = static_cast<uint32_t>(widthIn);
            bufParamsIn.dim_y     = static_cast<uint32_t>(heightIn);
            bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

            bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
            bufParamsOut.dim_x     = static_cast<uint32_t>(widthOut);
            bufParamsOut.dim_y     = static_cast<uint32_t>(heightOut);
            bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
            /******************************************************************************/
            /* Validate parameters before init                                            */
            /******************************************************************************/

            AUDIOLIB_STATUS checkRet =
                AUDIOLIB_ssrc_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(checkRet, "AUDIOLIB_ssrc_init_checkParams");
            /******************************************************************************/
            /* Call init function from AUDIOLIB to for ssrc to set SE/SA params */
            /******************************************************************************/

            if (errorCtx.isSuccess()) {
               AUDIOLIB_STATUS initRet = AUDIOLIB_ssrc_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
               if (errorCtx.isSuccess()) {
                  AUDIOLIB_STATUS setRet =
                      AUDIOLIB_ssrc_set(_handle.get(), static_cast<uint8_t>(AUDIOLIB_SSRC_MODE_RESET), _pX, _pState);
                  errorCtx.remapXLIBError(setRet, "AUDIOLIB_ssrc_set");
               }
            }
         }
      }
   }
}

/* ssrc setAddr */

template <typename T> void *Ssrc<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Ssrc<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Ssrc<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Ssrc<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Ssrc<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Ssrc<T>::exec() noexcept
{
   AUDIOLIB_ssrc_exec(_handle.get(), _pX, _pState, _pFiltCoeffs, _pY);
}

template <typename T>
Ssrc<T>::Ssrc(T          *pX,
              T          *pState,
              T          *pFiltCoeffs,
              T          *pY,
              size_t      inputSampleCount,
              size_t      sampleDataType,
              size_t      widthIn,
              size_t      heightIn,
              size_t      inPitchBytes,
              size_t      widthOut,
              size_t      heightOut,
              size_t      outPitchBytes,
              size_t      inputSampleRate,
              size_t      outputSampleRate,
              size_t      numChannels,
              size_t      isInterleave,
              size_t      bufferFormat,
              size_t      enableMMA,
              size_t      mmaSize,
              const char *nodeName,
              uint32_t    nodeId,
              ErrorCtxt  &errorCtx)
    : Ssrc(pX,
           pState,
           pFiltCoeffs,
           pY,
           inputSampleCount,
           sampleDataType,
           widthIn,
           heightIn,
           inPitchBytes,
           widthOut,
           heightOut,
           outPitchBytes,
           inputSampleRate,
           outputSampleRate,
           numChannels,
           isInterleave,
           bufferFormat,
           enableMMA,
           mmaSize,
           errorCtx)
{
   (void) pX;
   (void) pState;
   (void) pFiltCoeffs;
   (void) pY;
   (void) inputSampleCount;
   (void) sampleDataType;
   (void) widthIn;
   (void) heightIn;
   (void) inPitchBytes;
   (void) widthOut;
   (void) heightOut;
   (void) outPitchBytes;
   (void) inputSampleRate;
   (void) outputSampleRate;
   (void) numChannels;
   (void) isInterleave;
   (void) bufferFormat;
   (void) enableMMA;
   (void) mmaSize;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Ssrc<float>;

} // namespace AUDIOLIB
} // namespace TISP
