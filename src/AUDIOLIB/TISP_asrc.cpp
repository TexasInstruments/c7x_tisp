// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_asrc.hpp>
#include <cstddef>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

/* asrc constructor */

template <typename T>
Asrc<T>::Asrc(T         *pX,
              T         *pNonInterleavedData,
              T         *pFiltCoeffs,
              T         *pFilterRembuf,
              T         *pY,
              size_t     widthIn,
              size_t     heightIn,
              size_t     inPitchBytes,
              size_t     widthOut,
              size_t     heightOut,
              size_t     outPitchBytes,
              size_t     maxSampleCountPerBlock,
              size_t     outputSampleCount,
              size_t     sampleDataType,
              size_t     inputSampleRate,
              size_t     outputSampleRate,
              size_t     numChannels,
              size_t     frameModuloFactor,
              size_t     isInterleave,
              ErrorCtxt &errorCtx)
    : _pX(pX), _pNonInterleavedData(pNonInterleavedData), _pFiltCoeffs(pFiltCoeffs), _pFilterRembuf(pFilterRembuf),
      _pY(pY)
{

   /* Initialize _params with the node parameters */
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "widthIn=%zu,heightIn=%zu,widthOut=%zu,heightOut=%zu,isInterleave=%zu", widthIn, heightIn, widthOut,
                   heightOut, isInterleave);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Asrc constructor", "input pointer pX is null");
   }
   else if (pNonInterleavedData == nullptr && isInterleave == 1) {
      errorCtx.setNullPointerError("Asrc constructor", "non interleaved data pointer pNonInterleavedData is null");
   }
   else if (pFiltCoeffs == nullptr) {
      errorCtx.setNullPointerError("Asrc constructor", "destination buffer pointer pFiltCoeffs is null");
   }
   else if (pFilterRembuf == nullptr) {
      errorCtx.setNullPointerError("Asrc constructor", "remaining filter buffer pointer pFilterRembuf is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("Asrc constructor", "output pointer pY is null");
   }
   else if (widthIn == 0) {
      errorCtx.setInvalidParameterError("Asrc constructor", "widthIn must be greater than zero");
   }
   else if (heightIn == 0) {
      errorCtx.setInvalidParameterError("Asrc constructor", "heightIn must be greater than zero");
   }
   else if (widthOut == 0) {
      errorCtx.setInvalidParameterError("Asrc constructor", "widthOut must be greater than zero");
   }
   else if (heightOut == 0) {
      errorCtx.setInvalidParameterError("Asrc constructor", "heightOut must be greater than zero");
   }
   else if (maxSampleCountPerBlock == 0) {
      errorCtx.setInvalidParameterError("Asrc constructor", "maxSampleCountPerBlock must be greater than zero");
   }
   else if (inputSampleRate > 15) {
      errorCtx.setInvalidParameterError("Asrc constructor", "inputSampleRate must be less than 192000(15)");
   }
   else if (outputSampleRate > 15) {
      errorCtx.setInvalidParameterError("Asrc constructor", "statisticsType must be less than 192000(15)");
   }
   else if (numChannels == 0) {
      errorCtx.setInvalidParameterError("Asrc constructor", "numChannels must be greater than zero");
   }
   else if (frameModuloFactor == 0) {
      errorCtx.setInvalidParameterError("Asrc constructor", "frameModuloFactor must be greater than zero");
   }
   else if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("Asrc constructor", "isInterleave must be either zero or one");
   }
   else {
      _inputSampleCount  = maxSampleCountPerBlock;
      _outputSampleCount = outputSampleCount;

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
      /* Initialize data structures required by AUDIOLIB_asrc from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_asrc_InitArgs kerInitArgs;
      kerInitArgs.funcStyle              = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.maxSampleCountPerBlock = maxSampleCountPerBlock;
      kerInitArgs.sampleDataType         = sampleDataType;
      kerInitArgs.inputSampleRate        = static_cast<sample_rate_t>(inputSampleRate);
      kerInitArgs.outputSampleRate       = static_cast<sample_rate_t>(outputSampleRate);
      kerInitArgs.numChannels            = numChannels;
      kerInitArgs.dataFormat             = isInterleave;
      kerInitArgs.frameModuloFactor      = frameModuloFactor;
      kerInitArgs.matTransHandle         = nullptr;

      if (kerInitArgs.dataFormat == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
         _matTransHandle = make_unique_handle<char>(DSPLIB_matTrans_getHandleSize(nullptr));
         if (!_matTransHandle) {
            errorCtx.setMemoryError("Asrc constructor", "failed to allocate matTrans handle");
         }

         if (errorCtx.isSuccess()) {
            kerInitArgs.matTransHandle = _matTransHandle.get();
         }
      }

      if (errorCtx.isSuccess()) {

         _handle = make_unique_handle<char>(AUDIOLIB_asrc_getHandleSize(&kerInitArgs));
         if (!_handle) {
            errorCtx.setMemoryError("Asrc constructor", "failed to allocate asrc handle");
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

            AUDIOLIB_STATUS status =
                AUDIOLIB_asrc_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_asrc_init_checkParams");
            /******************************************************************************/
            /* Call init function from AUDIOLIB to for asrc to set SE/SA params             */
            /******************************************************************************/
            if (errorCtx.isSuccess()) {
               status = AUDIOLIB_asrc_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
               if (errorCtx.isSuccess()) {
                  float fsout =
                      static_cast<float>(convertSampleRateToInt(static_cast<sample_rate_t>(outputSampleRate)));
                  float fsin = static_cast<float>(convertSampleRateToInt(static_cast<sample_rate_t>(inputSampleRate)));
                  float asrcRatio = fsout / fsin;
                  status =
                      AUDIOLIB_asrc_set(_handle.get(), AUDIOLIB_ASRC_MODE_RESET, asrcRatio, _pNonInterleavedData, _pX);
                  errorCtx.remapXLIBError(status, "AUDIOLIB_asrc_set");
               }
            }
         }
      }
   }
}

template <typename T> bool Asrc<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Asrc::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<AsrcParamId>(paramId)) {
      case AsrcParamId::kMode: {
         if (size < sizeof(TISP_asrcMode)) {
            errorCtx.setInvalidParameterError("Asrc::setParams", "Value size smaller than expected for kMode");
         }
         else {
            const auto *pNewConfig = static_cast<const TISP_asrcMode *>(value);

            AUDIOLIB_STATUS status;
            if (pNewConfig->mode == AUDIOLIB_ASRC_MODE_RESET) {
               status =
                   AUDIOLIB_asrc_set(_handle.get(), pNewConfig->mode, pNewConfig->asrcRatio, _pNonInterleavedData, _pX);
            }
            else {
               status = AUDIOLIB_asrc_set(_handle.get(), pNewConfig->mode, pNewConfig->asrcRatio, nullptr, nullptr);
            }
            (void) errorCtx.remapXLIBError(status, "AUDIOLIB_asrc_set");

            if (errorCtx.isSuccess()) {
               _asrcConfig = *pNewConfig;
            }
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Asrc::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> bool Asrc<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Asrc::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("Asrc::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<AsrcParamId>(paramId)) {
      case AsrcParamId::kMode: {
         if (*size < sizeof(TISP_asrcMode)) {
            errorCtx.setInvalidParameterError("Asrc::getParams", "Provided buffer size is too small for kMode");
         }
         else {
            memcpy(value, &_asrcConfig, sizeof(TISP_asrcMode));
         }
         *size = sizeof(TISP_asrcMode);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Asrc::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> void *Asrc<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Asrc<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Asrc<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Asrc<T>::getNumOutputs() const noexcept { return 1; }

/* Asrc setAddr */
template <typename T> void Asrc<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> size_t Asrc<T>::getNumOutputSampleCount() const noexcept { return _outputSampleCount; }

template <typename T> void Asrc<T>::exec() noexcept
{
   AUDIOLIB_asrc_ExecInArgs kerInArgs;
   kerInArgs.inputSampleCount = _inputSampleCount;

   AUDIOLIB_asrc_ExecOutArgs kerOutArgs;
   kerOutArgs.outputSampleCount = _outputSampleCount;

   AUDIOLIB_asrc_exec(_handle.get(), _pX, _pNonInterleavedData, _pFiltCoeffs, _pFilterRembuf, _pY, &kerInArgs,
                      &kerOutArgs);
   _outputSampleCount = kerOutArgs.outputSampleCount;
}

template <typename T>
Asrc<T>::Asrc(T          *pX,
              T          *pNonInterleavedData,
              T          *pFiltCoeffs,
              T          *pFilterRembuf,
              T          *pY,
              size_t      widthIn,
              size_t      heightIn,
              size_t      inPitchBytes,
              size_t      widthOut,
              size_t      heightOut,
              size_t      outPitchBytes,
              size_t      maxSampleCountPerBlock,
              size_t      outputSampleCount,
              size_t      sampleDataType,
              size_t      inputSampleRate,
              size_t      outputSampleRate,
              size_t      numChannels,
              size_t      frameModuloFactor,
              size_t      isInterleave,
              const char *nodeName,
              uint32_t    nodeId,
              ErrorCtxt  &errorCtx)
    : Asrc(pX,
           pNonInterleavedData,
           pFiltCoeffs,
           pFilterRembuf,
           pY,
           widthIn,
           heightIn,
           inPitchBytes,
           widthOut,
           heightOut,
           outPitchBytes,
           maxSampleCountPerBlock,
           outputSampleCount,
           sampleDataType,
           inputSampleRate,
           outputSampleRate,
           numChannels,
           frameModuloFactor,
           isInterleave,
           errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Asrc<float>;

} // namespace AUDIOLIB
} // namespace TISP
