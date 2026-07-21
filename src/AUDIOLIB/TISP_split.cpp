// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <TISP_split.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
Split<T>::Split(T                    *pIn,
                size_t                inPitchBytes,
                const TISP_BufDesc2D *pOutputs,
                uint32_t              numOutputs,
                uint32_t              inSamples,
                uint8_t               isInterleave,
                ErrorCtxt            &errorCtx)
    : _pIn(pIn), _numOutputs(numOutputs), _totalInputChannels(0), _inSamples(inSamples), _isInterleave(isInterleave)
{
   // Allocate output pointer array from heap section
   _outputPtrs = std::make_unique<void*[]>(numOutputs); 

   if (!_outputPtrs) {
      errorCtx.setMemoryError("Split constructor", "failed to allocate outputPtrs array");
   }

   if (pOutputs == nullptr) {
      errorCtx.setNullPointerError("Split constructor", "output descriptor array pOutputs is null");
   }
   else if (errorCtx.isSuccess()) {
      for (uint32_t i = 0; i < numOutputs; i++) {
         _outputPtrs[i] = pOutputs[i].pBuf;
         _totalInputChannels += (isInterleave ? pOutputs[i].width : pOutputs[i].height); // channels
      }
   }

   // Input validation checks
   if (pIn == nullptr) {
      errorCtx.setNullPointerError("Split constructor", "input pointer pIn is null");
   }
   if (pOutputs != nullptr) {
      for (uint32_t i = 0; i < numOutputs; i++) {
         if (pOutputs[i].pBuf == nullptr) {
            errorCtx.setNullPointerError("Split constructor", "output pointer pBuf is null");
         }
         uint32_t channels = isInterleave ? pOutputs[i].width : pOutputs[i].height;
         if (channels == 0) {
            errorCtx.setInvalidParameterError("Split constructor", "output channels must be greater than zero");
         }
         if (pOutputs[i].stride_y <= 0) {
            errorCtx.setInvalidParameterError("Split constructor", "output stride_y must be greater than zero");
         }
      }
   }
   if (numOutputs == 0) {
      errorCtx.setInvalidParameterError("Split constructor", "numOutputs must be greater than zero");
   }
   if (numOutputs > static_cast<uint32_t>(kMaxDstAddr)) {
      errorCtx.setInvalidParameterError("Split constructor", "numOutputs must not exceed kMaxDstAddr");
   }
   if (inSamples == 0) {
      errorCtx.setInvalidParameterError("Split constructor", "inSamples must be greater than zero");
   }
   if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("Split constructor", "interleave must be either zero or one");
   }

   // Build params string for profiler / getParams() after validation, so _totalInputChannels is final.
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numOutputs=%u,totalInputChannels=%u,inSamples=%u",
                   numOutputs, _totalInputChannels, inSamples);

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

      // Initialize bufParams with node input and output buffer parameters. Split fans a
      // single input out to numOutputs destinations; the caller pre-orients each output
      // descriptor's width/height per isInterleave, so map them straight through (populate
      // up to kMaxDstAddr output slots; validation above guarantees numOutputs <= kMaxDstAddr).
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      if (isInterleave) {
         _nodeInfo.bufParams.widthIn[0]  = _totalInputChannels; // Channels on X (width)
         _nodeInfo.bufParams.heightIn[0] = inSamples;           // Samples on Y (height)
      }
      else {
         _nodeInfo.bufParams.widthIn[0]  = inSamples;           // Samples on X (width)
         _nodeInfo.bufParams.heightIn[0] = _totalInputChannels; // Channels on Y (height)
      }
      _nodeInfo.bufParams.inPitchBytes[0] = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      size_t numBufOut =
          (numOutputs < static_cast<uint32_t>(kMaxDstAddr)) ? numOutputs : static_cast<uint32_t>(kMaxDstAddr);
      for (size_t i = 0; i < numBufOut; i++) {
         _nodeInfo.bufParams.widthOut[i]      = pOutputs[i].width;
         _nodeInfo.bufParams.heightOut[i]     = pOutputs[i].height;
         _nodeInfo.bufParams.outPitchBytes[i] = static_cast<size_t>(pOutputs[i].stride_y);
      }

      // Build AUDIOLIB init args. Heap-allocated temporary: only needed during init,
      // using make_unique avoids polluting the handle arena with a non-LIFO block.
      auto outChannels = std::make_unique<uint32_t[]>(numOutputs);
      for (uint32_t i = 0; i < numOutputs; i++) {
         outChannels[i] = isInterleave ? pOutputs[i].width : pOutputs[i].height;
      }
      {
         AUDIOLIB_split_InitArgs initArgs;
         initArgs.funcStyle          = AUDIOLIB_FUNCTION_OPTIMIZED;
         initArgs.outChannels        = outChannels.get();
         initArgs.numOutputs         = numOutputs;
         initArgs.totalInputChannels = _totalInputChannels;
         initArgs.isInputInterleave  = isInterleave;

         // Allocate opaque kernel handle
         _handle = make_unique_handle<char>(AUDIOLIB_split_getHandleSize(&initArgs));

         if (!_handle) {
            errorCtx.setMemoryError("Split constructor", "failed to allocate split handle");
         }

         if (errorCtx.isSuccess()) {
            // Input buffer params — swap dims based on isInterleave
            AUDIOLIB_bufParams2D_t bufParamsIn;
            bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
            if (isInterleave) {
               bufParamsIn.dim_x = static_cast<uint32_t>(_totalInputChannels); // Channels on X (width)
               bufParamsIn.dim_y = static_cast<uint32_t>(inSamples);           // Samples on Y (height)
            }
            else {
               bufParamsIn.dim_x = static_cast<uint32_t>(inSamples);           // Samples on X (width)
               bufParamsIn.dim_y = static_cast<uint32_t>(_totalInputChannels); // Channels on Y (height)
            }
            bufParamsIn.stride_y = static_cast<int32_t>(inPitchBytes);

            // Output buffer params — taken from the per-output descriptors.
            // Owned via unique_ptr so it is freed when the constructor returns.
            auto bufParamsOut = std::make_unique<AUDIOLIB_bufParams2D_t[]>(_numOutputs);

            if (errorCtx.isSuccess()) {
               for (uint32_t i = 0; i < _numOutputs; i++) {
                  // Caller pre-orients width/height per isInterleave; map straight through.
                  bufParamsOut[i].data_type = AUDIOLIB_element_dType<T>::value;
                  bufParamsOut[i].dim_x     = pOutputs[i].width;
                  bufParamsOut[i].dim_y     = pOutputs[i].height;
                  bufParamsOut[i].stride_y  = pOutputs[i].stride_y;
               }

               /******************************************************************************/
               /* Validate parameters before init                                            */
               /******************************************************************************/
               AUDIOLIB_STATUS checkRet =
                   AUDIOLIB_split_init_checkParams(_handle.get(), &bufParamsIn, bufParamsOut.get(), &initArgs);
               errorCtx.remapXLIBError(checkRet, "AUDIOLIB_split_init_checkParams");

               /******************************************************************************/
               /* Call init function from AUDIOLIB to for split to set SE/SA params             */
               /******************************************************************************/

               if (errorCtx.isSuccess()) {
                  AUDIOLIB_STATUS initRet =
                      AUDIOLIB_split_init(_handle.get(), &bufParamsIn, bufParamsOut.get(), &initArgs);
                  errorCtx.remapXLIBError(initRet, "AUDIOLIB_split_init");
               }
            }
         }
      }
   }
}

template <typename T>
Split<T>::Split(T                    *pIn,
                size_t                inPitchBytes,
                const TISP_BufDesc2D *pOutputs,
                uint32_t              numOutputs,
                uint32_t              inSamples,
                uint8_t               isInterleave,
                const char           *nodeName,
                uint32_t              nodeId,
                ErrorCtxt            &errorCtx)
    : Split(pIn, inPitchBytes, pOutputs, numOutputs, inSamples, isInterleave, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Split<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pIn) : nullptr;
}

template <typename T> void *Split<T>::getOutputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   if (index < _numOutputs) {
      result = _outputPtrs[index];
   }
   return result;
}

template <typename T> size_t Split<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Split<T>::getNumOutputs() const noexcept { return _numOutputs; }

template <typename T> void Split<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeOut || mode == kAddrModeInOut) {
      for (uint32_t i = 0; i < _numOutputs; i++)
         _outputPtrs[i] = addrs.dst[i];
   }
   if (mode == kAddrModeIn || mode == kAddrModeInOut) {
      _pIn = static_cast<T *>(addrs.src[0]);
   }
}

template <typename T> void Split<T>::exec() noexcept
{
   AUDIOLIB_split_exec(_handle.get(), static_cast<void *>(_pIn), _outputPtrs.get());
}

template class Split<float>;
template class Split<double>;

} // namespace AUDIOLIB
} // namespace TISP
