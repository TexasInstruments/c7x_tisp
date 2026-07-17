// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include "common/AUDIOLIB_types.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_router.hpp>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Router constructor]

template <typename T>
Router<T>::Router(T         *pX,
                  T         *pC,
                  T         *pS,
                  T         *pY,
                  size_t     width,
                  size_t     height,
                  size_t     outChannels,
                  size_t     inPitchBytes,
                  size_t     outPitchBytes,
                  size_t     isInterleave,
                  ErrorCtxt &errorCtx)
    : _pX(pX), _pC(pC), _pS(pS), _pY(pY)
{

   // TODO: See if we can print the routing table from buffer here
   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", width, height);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Router constructor", "input pointer pX is null");
   }
   else if (pC == nullptr) {
      errorCtx.setNullPointerError("Router constructor", "channel index array pC is null");
   }
   else if (pS == nullptr) {
      errorCtx.setNullPointerError("Router constructor", "output scratch pointer pS is null");
   }
   else if (pY == nullptr) {
      errorCtx.setNullPointerError("Router constructor", "output pointer pY is null");
   }
   else if (width == 0) {
      errorCtx.setInvalidParameterError("Router constructor", "width must be greater than zero");
   }
   else if (height == 0) {
      errorCtx.setInvalidParameterError("Router constructor", "height must be greater than zero");
   }
   else if (outChannels == 0) {
      errorCtx.setInvalidParameterError("Router constructor", "outChannels must be greater than zero");
   }
   else if (isInterleave > 1) {
      errorCtx.setInvalidParameterError("Router constructor", "interleave must be either zero or one");
   }
   else {
      _width         = width;
      _height        = height;
      _outChannels   = outChannels;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _isInterleave  = isInterleave;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = isInterleave;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = isInterleave;
      _nodeInfo.bufParams.widthOut[0]        = (isInterleave == 1) ? outChannels : width;
      _nodeInfo.bufParams.heightOut[0]       = (isInterleave == 1) ? height : outChannels;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Router from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_router_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = isInterleave;
      kerInitArgs.numOutputs   = outChannels;

      _handle = make_unique_handle<char>(AUDIOLIB_router_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Router constructor", "failed to allocate router handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams2D_t bufParamsIn0;
         AUDIOLIB_bufParams2D_t bufParamsOut;
         AUDIOLIB_bufParams1D_t bufParamsIn1;

         bufParamsIn0.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn0.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn0.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn0.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         if (isInterleave == 1) {
            bufParamsOut.dim_x = static_cast<uint32_t>(outChannels);
            bufParamsOut.dim_y = static_cast<uint32_t>(height);
         }
         else {
            bufParamsOut.dim_x = static_cast<uint32_t>(width);
            bufParamsOut.dim_y = static_cast<uint32_t>(outChannels);
         }
         bufParamsOut.stride_y = static_cast<int32_t>(outPitchBytes);

         bufParamsIn1.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn1.dim_x     = static_cast<uint32_t>(outChannels);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status =
             AUDIOLIB_router_init_checkParams(_handle.get(), &bufParamsIn0, &bufParamsIn1, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(status, "AUDIOLIB_router_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for router to set SE/SA params         */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            status = AUDIOLIB_router_init(_handle.get(), &bufParamsIn0, &bufParamsIn1, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_router_init");
            if (errorCtx.isSuccess()) {
               status = AUDIOLIB_router_set(_handle.get(), _pC);
               errorCtx.remapXLIBError(status, "AUDIOLIB_router_set");
            }
         }
      }
   }
}

template <typename T> void *Router<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Router<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Router<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Router<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Router setAddr]
template <typename T> void Router<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Router<T>::exec() noexcept { AUDIOLIB_router_exec(_handle.get(), _pX, _pY, _pS); }

template <typename T>
Router<T>::Router(T          *pX,
                  T          *pC,
                  T          *pS,
                  T          *pY,
                  size_t      width,
                  size_t      height,
                  size_t      outChannels,
                  size_t      inPitchBytes,
                  size_t      outPitchBytes,
                  size_t      isInterleave,
                  const char *nodeName,
                  uint32_t    nodeId,
                  ErrorCtxt  &errorCtx)
    : Router(pX, pC, pS, pY, width, height, outChannels, inPitchBytes, outPitchBytes, isInterleave, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T>
bool Router<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Router::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<RouterParamId>(paramId)) {
      case RouterParamId::kChannelIndices: {
         if (size < _outChannels * sizeof(T)) {
            errorCtx.setInvalidParameterError("Router::setParams",
                                              "Value size smaller than expected for kChannelIndices");
         }
         else {
            auto pCTemp = std::make_unique<T[]>(_outChannels);
            if (!pCTemp) {
               errorCtx.setMemoryError("Router::setParams", "failed to allocate temp buffer");
            }
            else {
               memcpy(pCTemp.get(), value, _outChannels * sizeof(T));
               AUDIOLIB_STATUS status = AUDIOLIB_router_set(_handle.get(), pCTemp.get());
               (void) errorCtx.remapXLIBError(status, "AUDIOLIB_router_set");
               if (errorCtx.isSuccess()) {
                  memcpy(_pC, pCTemp.get(), _outChannels * sizeof(T));
               }
            }
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Router::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool Router<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("Router::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("Router::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<RouterParamId>(paramId)) {
      case RouterParamId::kChannelIndices: {
         if (*size < _outChannels * sizeof(T)) {
            errorCtx.setInvalidParameterError("Router::getParams",
                                              "Provided buffer size is too small for kChannelIndices");
         }
         else {
            memcpy(value, _pC, _outChannels * sizeof(T));
         }
         *size = _outChannels * sizeof(T);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("Router::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class Router<float>;
template class Router<double>;

} // namespace AUDIOLIB
} // namespace TISP
