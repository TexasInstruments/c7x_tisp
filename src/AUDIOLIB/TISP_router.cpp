// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <TISP_router.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Router constructor]

template <typename T>
Router<T>::Router(const TISP_BufDescList       *pInputs,
                  const TISP_BufDescList       *pOutputs,
                  void                         *pScratch,
                  size_t                        scratchBytes,
                  const RouterTunableParams<T> *pDefaultTunableParams,
                  const char                   *nodeName,
                  uint32_t                      nodeId,
                  ErrorCtxt                    &errorCtx)
    : _in{}, _out{}, _numOutChannels(0U), _channelIndices(nullptr), _scratch(pScratch)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   (void) scratchBytes;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("Router constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pChannelIndices == nullptr) {
      errorCtx.setNullPointerError("Router constructor",
                                   "channel index pointer pDefaultTunableParams->pChannelIndices is null");
   }
   else if (pScratch == nullptr) {
      errorCtx.setNullPointerError("Router constructor", "scratch pointer pScratch is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Router constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Router constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("Router constructor", "input and output interleave must match");
      }
   }

   if (errorCtx.isSuccess()) {
      // One channel index per output channel: channels are columns when interleaved, rows otherwise
      _numOutChannels = (_out.isInterleave == 1U) ? _out.width : _out.height;
      _channelIndices = pDefaultTunableParams->pChannelIndices;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "width=%zu, height=%zu, numOutChannels=%zu, isInterleave=%u", _in.width, _in.height,
                      _numOutChannels, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_router from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_router_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.isInterleave = _in.isInterleave;
      kerInitArgs.numOutputs   = static_cast<uint32_t>(_numOutChannels);

      _handle = make_unique_handle<char>(AUDIOLIB_router_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Router constructor", "failed to allocate router handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<T>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<T>(_out);
         AUDIOLIB_bufParams1D_t bufParamsChannelIndex;

         // AUDIOLIB_router_init_checkParams requires the index buffer type to match the data type
         bufParamsChannelIndex.data_type = static_cast<uint32_t>(AUDIOLIB_element_dType<T>::value);
         bufParamsChannelIndex.dim_x     = static_cast<uint32_t>(_numOutChannels);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status = AUDIOLIB_router_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsChannelIndex,
                                                                   &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for router to set SE/SA params           */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "AUDIOLIB_router_init_checkParams")) {
            status = AUDIOLIB_router_init(_handle.get(), &bufParamsIn, &bufParamsChannelIndex, &bufParamsOut,
                                          &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "AUDIOLIB_router_init")) {
               applyChannelIndices(_channelIndices, "Router constructor", errorCtx);
            }
         }
      }
   }
}

template <typename T>
void Router<T>::applyChannelIndices(uint32_t *pChannelIndices, const char *funcName, ErrorCtxt &errorCtx) noexcept
{
   AUDIOLIB_STATUS status = AUDIOLIB_router_set(_handle.get(), pChannelIndices);

   if (errorCtx.remapXLIBError(status, funcName) && (pChannelIndices != _channelIndices)) {
      memcpy(_channelIndices, pChannelIndices, _numOutChannels * sizeof(uint32_t));
   }
}

template <typename T> void *Router<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *Router<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Router<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Router<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Router setAddr]
template <typename T> void Router<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addr.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void Router<T>::exec() noexcept
{
   AUDIOLIB_router_exec(_handle.get(), _in.pBuf, _out.pBuf, _scratch);
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
         if (size < (_numOutChannels * sizeof(uint32_t))) {
            errorCtx.setInvalidParameterError("Router::setParams",
                                              "Value size smaller than expected for kChannelIndices");
         }
         else {
            applyChannelIndices(static_cast<uint32_t *>(value), "AUDIOLIB_router_set", errorCtx);
         }
         break;
      }
      case RouterParamId::kAll: {
         const RouterTunableParams<T> *pParams = static_cast<const RouterTunableParams<T> *>(value);
         if (size < sizeof(RouterTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Router::setParams", "Value size smaller than expected for kAll");
         }
         else if (pParams->pChannelIndices == nullptr) {
            errorCtx.setNullPointerError("Router::setParams", "channel index pointer pChannelIndices is null");
         }
         else {
            applyChannelIndices(pParams->pChannelIndices, "AUDIOLIB_router_set", errorCtx);
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
   if (size == nullptr) {
      errorCtx.setNullPointerError("Router::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<RouterParamId>(paramId)) {
      case RouterParamId::kChannelIndices: {
         size_t expectedSize = _numOutChannels * sizeof(uint32_t);
         if (value == nullptr) {
            errorCtx.setNullPointerError("Router::getParams", "value pointer is null");
         }
         else if (*size < expectedSize) {
            errorCtx.setInvalidParameterError("Router::getParams",
                                              "Provided buffer size is too small for kChannelIndices");
         }
         else if (value != _channelIndices) {
            memcpy(value, _channelIndices, expectedSize);
         }
         else {
            /* Nothing to do here */
         }
         *size = expectedSize;
         break;
      }
      case RouterParamId::kAll: {
         RouterTunableParams<T> *pParams = static_cast<RouterTunableParams<T> *>(value);
         if (pParams == nullptr) {
            errorCtx.setNullPointerError("Router::getParams", "value pointer is null");
         }
         else if (*size < sizeof(RouterTunableParams<T>)) {
            errorCtx.setInvalidParameterError("Router::getParams", "Provided buffer size is too small for kAll");
         }
         else if (pParams->pChannelIndices == nullptr) {
            errorCtx.setNullPointerError("Router::getParams", "channel index pointer pChannelIndices is null");
         }
         else if (pParams->pChannelIndices != _channelIndices) {
            memcpy(pParams->pChannelIndices, _channelIndices, _numOutChannels * sizeof(uint32_t));
         }
         else {
            /* Nothing to do here */
         }
         *size = sizeof(RouterTunableParams<T>);
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
