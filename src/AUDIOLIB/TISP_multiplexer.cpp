// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include "common/AUDIOLIB_types.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_multiplexer.hpp>
#include <cstddef>
#include <cstdint>
#include <random>

namespace TISP {
namespace AUDIOLIB {

#define TISP_MUX_NPARAMS (1)

/* Multiplexer constructor */

template <typename T>
Multiplexer<T>::Multiplexer(T         *pX,
                            T         *pY,
                            size_t     width,
                            size_t     height,
                            size_t     inputs,
                            size_t     inPitchBytesY,
                            size_t     inPitchBytesZ,
                            size_t     outPitchBytes,
                            size_t     currentIdx,
                            ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{
   (void) inPitchBytesZ;

   /* Initialize _params with the node parameters */
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "widthIn=%zu,heightIn=%zu", width, height);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Multiplexer constructor", "input pointer pX is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("Multiplexer constructor", "output pointer pY is null");
   }
   else if (width == 0) {
      errorCtx.setInvalidParameterError("Multiplexer constructor", "width must be greater than zero");
   }
   else if (height == 0) {
      errorCtx.setInvalidParameterError("Multiplexer constructor", "height must be greater than zero");
   }
   else if (inputs == 0) {
      errorCtx.setInvalidParameterError("Multiplexer constructor", "inputs must be greater than zero");
   }
   else {
      _height        = height;
      _inputs        = inputs;
      _inPitchBytesY = inPitchBytesY;

      _inputIdx = (currentIdx >= _inputs) ? 0 : currentIdx;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytesY;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Multiplexer from AUDIOLIB */
      /***********************************************************************/

      DSPLIB_blkCopy2DInitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_blkCopy2D_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Multiplexer constructor", "failed to allocate blkCopy2D handle");
      }

      if (errorCtx.isSuccess()) {

         DSPLIB_bufParams2D_t bufParamsIn;
         DSPLIB_bufParams2D_t bufParamsOut;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytesY);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         DSPLIB_STATUS status =
             DSPLIB_blkCopy2D_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(status, "DSPLIB_blkCopy2D_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for multiplexer to set SE/SA params         */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            status = DSPLIB_blkCopy2D_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_blkCopy2D_init");
         }
      }
   }
}

template <typename T>
bool Multiplexer<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Multiplexer::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<MultiplexerParamId>(paramId)) {
      case MultiplexerParamId::kSrcIndex: {
         if (size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Multiplexer::setParams",
                                              "Value size smaller than expected for kSrcIndex");
         }
         else {
            uint32_t srcIndex = 0;
            memcpy(&srcIndex, value, sizeof(uint32_t));
            if (srcIndex < _inputs) {
               _inputIdx = srcIndex;
            }
            else {
               errorCtx.setInvalidParameterError("Multiplexer::setParams", "srcIndex out of bounds");
            }
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Multiplexer::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool Multiplexer<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Multiplexer::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("Multiplexer::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<MultiplexerParamId>(paramId)) {
      case MultiplexerParamId::kSrcIndex: {
         if (*size < sizeof(uint32_t)) {
            errorCtx.setInvalidParameterError("Multiplexer::getParams", "Provided buffer size is too small");
         }
         else {
            uint32_t srcIndex = static_cast<uint32_t>(_inputIdx);
            memcpy(value, &srcIndex, sizeof(uint32_t));
         }
         *size = sizeof(uint32_t);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Multiplexer::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

/* Multiplexer setAddr */

template <typename T> void *Multiplexer<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Multiplexer<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Multiplexer<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Multiplexer<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Multiplexer<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<T *>(addr.src[0]) + (_inputIdx * _height * _inPitchBytesY / sizeof(T));
   }
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<T *>(addr.src[0]) + (_inputIdx * _height * _inPitchBytesY / sizeof(T));
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void Multiplexer<T>::exec() noexcept { DSPLIB_blkCopyConst2D_exec(_handle.get(), _pX, _pY); }

template <typename T>
Multiplexer<T>::Multiplexer(T          *pX,
                            T          *pY,
                            size_t      width,
                            size_t      height,
                            size_t      inputs,
                            size_t      inPitchBytesY,
                            size_t      inPitchBytesZ,
                            size_t      outPitchBytes,
                            size_t      currentIdx,
                            const char *nodeName,
                            uint32_t    nodeId,
                            ErrorCtxt  &errorCtx)
    : Multiplexer(pX, pY, width, height, inputs, inPitchBytesY, inPitchBytesZ, outPitchBytes, currentIdx, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Multiplexer<float>;
template class Multiplexer<double>;

} // namespace AUDIOLIB
} // namespace TISP
