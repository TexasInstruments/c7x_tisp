// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <TISP_concat.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
Concat<T>::Concat(const TISP_BufDesc2D *pInputs,
                  uint32_t              numInputs,
                  T                    *pY,
                  size_t                outPitchBytes,
                  uint32_t              inSamples,
                  uint8_t               isInterleave,
                  ErrorCtxt            &errorCtx)
    : _pY(pY), _numInputs(numInputs), _totalInChannels(0), _inSamples(inSamples), _isInterleave(isInterleave)
{
   // Copy input pointers into owned void* array
   _inputPtrs = make_unique_handle<void *>(numInputs);
   if (!_inputPtrs) {
      errorCtx.setMemoryError("Concat constructor", "failed to allocate inputPtrs array");
   }

   if (errorCtx.isSuccess()) {
      if (pInputs == nullptr) {
         errorCtx.setNullPointerError("Concat constructor", "input descriptor array pInputs is null");
      }
      else {
         for (uint32_t i = 0; i < numInputs; i++) {
            _inputPtrs[i] = pInputs[i].pBuf;
            _totalInChannels += (isInterleave ? pInputs[i].width : pInputs[i].height); // channels
         }
      }
   }

   // Input validation checks
   if (pInputs != nullptr) {
      for (uint32_t i = 0; i < numInputs; i++) {
         if (pInputs[i].pBuf == nullptr) {
            errorCtx.setNullPointerError("Concat constructor", "input pointer pBuf is null");
         }
         uint32_t channels = isInterleave ? pInputs[i].width : pInputs[i].height;
         if (channels == 0) {
            errorCtx.setInvalidParameterError("Concat constructor", "input channels must be greater than zero");
         }
         if (pInputs[i].stride_y <= 0) {
            errorCtx.setInvalidParameterError("Concat constructor", "input stride_y must be greater than zero");
         }
      }
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Concat constructor", "output pointer pY is null");
   }
   if (numInputs == 0) {
      errorCtx.setInvalidParameterError("Concat constructor", "numInputs must be greater than zero");
   }
   if (numInputs > static_cast<uint32_t>(kMaxSrcAddr)) {
      errorCtx.setInvalidParameterError("Concat constructor", "numInputs must not exceed kMaxSrcAddr");
   }
   if (inSamples == 0) {
      errorCtx.setInvalidParameterError("Concat constructor", "inSamples must be greater than zero");
   }
   if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("Concat constructor", "interleave must be either zero or one");
   }

   // Build params string for profiler / getParams() after validation, so _totalInChannels is final.
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numInputs=%u,totalInChannels=%u,inSamples=%u",
                   numInputs, _totalInChannels, inSamples);

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

      // Initialize bufParams with node input and output buffer parameters. Concat has one
      // geometry per input source; the caller pre-orients each descriptor's width/height
      // per isInterleave, so map them straight through (validation above guarantees
      // numInputs does not exceed kMaxSrcAddr).
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      for (uint32_t i = 0; i < _numInputs; i++) {
         _nodeInfo.bufParams.widthIn[i]      = pInputs[i].width;
         _nodeInfo.bufParams.heightIn[i]     = pInputs[i].height;
         _nodeInfo.bufParams.inPitchBytes[i] = static_cast<size_t>(pInputs[i].stride_y);
      }

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      if (isInterleave) {
         _nodeInfo.bufParams.widthOut[0]  = _totalInChannels;
         _nodeInfo.bufParams.heightOut[0] = inSamples;
      }
      else {
         _nodeInfo.bufParams.widthOut[0]  = inSamples;
         _nodeInfo.bufParams.heightOut[0] = _totalInChannels;
      }
      _nodeInfo.bufParams.outPitchBytes[0] = outPitchBytes;

      // Build AUDIOLIB init args. Heap-allocated temporary: only needed during init,
      // using make_unique avoids polluting the handle arena with a non-LIFO block.
      auto inChannels = std::make_unique<uint32_t[]>(numInputs);
      for (uint32_t i = 0; i < numInputs; i++) {
         inChannels[i] = isInterleave ? pInputs[i].width : pInputs[i].height;
      }
      {
         AUDIOLIB_concat_InitArgs initArgs;
         initArgs.funcStyle       = AUDIOLIB_FUNCTION_OPTIMIZED;
         initArgs.inChannels      = inChannels.get();
         initArgs.numInputs       = numInputs;
         initArgs.totalInChannels = _totalInChannels;
         initArgs.isInterleave    = isInterleave;

         // Allocate opaque kernel handle
         _handle = make_unique_handle<char>(AUDIOLIB_concat_getHandleSize(&initArgs));

         if (!_handle) {
            errorCtx.setMemoryError("Concat constructor", "failed to allocate concat handle");
         }

         if (errorCtx.isSuccess()) {
            // Input buffer params — taken from the per-input descriptors.
            // Owned via unique_ptr so it is freed when the constructor returns.
            auto bufParamsIn = std::make_unique<AUDIOLIB_bufParams2D_t[]>(_numInputs);
            if (!bufParamsIn) {
               errorCtx.setMemoryError("Concat constructor", "failed to allocate bufParamsIn array");
            }

            if (errorCtx.isSuccess()) {
               for (uint32_t i = 0; i < _numInputs; i++) {
                  // Caller pre-orients width/height per isInterleave; map straight through.
                  bufParamsIn[i].data_type = AUDIOLIB_element_dType<T>::value;
                  bufParamsIn[i].dim_x     = pInputs[i].width;
                  bufParamsIn[i].dim_y     = pInputs[i].height;
                  bufParamsIn[i].stride_y  = pInputs[i].stride_y;
               }

               // Output buffer params — similar logic
               AUDIOLIB_bufParams2D_t bufParamsOut;
               bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
               if (isInterleave) {
                  bufParamsOut.dim_x = static_cast<uint32_t>(_totalInChannels); // Total channels
                  bufParamsOut.dim_y = static_cast<uint32_t>(inSamples);        // Samples
               }
               else {
                  bufParamsOut.dim_x = static_cast<uint32_t>(inSamples);        // Samples
                  bufParamsOut.dim_y = static_cast<uint32_t>(_totalInChannels); // Total channels
               }
               bufParamsOut.stride_y = static_cast<int32_t>(outPitchBytes);

               /******************************************************************************/
               /* Validate parameters before init                                            */
               /******************************************************************************/
               AUDIOLIB_STATUS checkRet =
                   AUDIOLIB_concat_init_checkParams(_handle.get(), bufParamsIn.get(), &bufParamsOut, &initArgs);
               errorCtx.remapXLIBError(checkRet, "AUDIOLIB_concat_init_checkParams");

               /******************************************************************************/
               /* Call init function from AUDIOLIB to for concat to set SE/SA params             */
               /******************************************************************************/

               if (errorCtx.isSuccess()) {
                  AUDIOLIB_STATUS initRet =
                      AUDIOLIB_concat_init(_handle.get(), bufParamsIn.get(), &bufParamsOut, &initArgs);
                  errorCtx.remapXLIBError(initRet, "AUDIOLIB_concat_init");
               }
            }
         }
      }
   }
}

template <typename T>
Concat<T>::Concat(const TISP_BufDesc2D *pInputs,
                  uint32_t              numInputs,
                  T                    *pY,
                  size_t                outPitchBytes,
                  uint32_t              inSamples,
                  uint8_t               isInterleave,
                  const char           *nodeName,
                  uint32_t              nodeId,
                  ErrorCtxt            &errorCtx)
    : Concat(pInputs, numInputs, pY, outPitchBytes, inSamples, isInterleave, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Concat<T>::getInputAddr(size_t index) const noexcept
{
   void *result = nullptr;
   if (index < _numInputs) {
      result = _inputPtrs[index];
   }
   return result;
}

template <typename T> void *Concat<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Concat<T>::getNumInputs() const noexcept { return _numInputs; }

template <typename T> size_t Concat<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Concat<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn || mode == kAddrModeInOut) {
      for (uint32_t i = 0; i < _numInputs; i++)
         _inputPtrs[i] = addrs.src[i];
   }
   if (mode == kAddrModeOut || mode == kAddrModeInOut)
      _pY = static_cast<T *>(addrs.dst[0]);
}

template <typename T> void Concat<T>::exec() noexcept
{
   AUDIOLIB_concat_exec(_handle.get(), _inputPtrs.get(), static_cast<void *>(_pY));
}

template class Concat<float>;
template class Concat<double>;

} // namespace AUDIOLIB
} // namespace TISP
