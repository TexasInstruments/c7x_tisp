// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_bitwiseANDConst.hpp>
#include <cstdio>
#include <cstring>

namespace TISP {
namespace DSPLIB {

template <typename T>
BitwiseANDConst<T>::BitwiseANDConst(const TISP_BufDescList                *pInputs,
                                    const TISP_BufDescList                *pOutputs,
                                    const BitwiseANDConstTunableParams<T> *pDefaultTunableParams,
                                    const char                            *nodeName,
                                    uint32_t                               nodeId,
                                    ErrorCtxt                             &errorCtx)
    : _in{}, _out{}, _constant(0)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("BitwiseANDConst constructor",
                                   "default params pointer pDefaultTunableParams is null");
   }
   else {
      /* Nothing to do here */
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "BitwiseANDConst constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "BitwiseANDConst constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("BitwiseANDConst constructor", "input and output interleave must match");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _constant = pDefaultTunableParams->constant;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,isInterleave=%u",
                      _in.width, _in.height, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      DSPLIB_bitwiseANDConst_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_bitwiseANDConst_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("BitwiseANDConst constructor", "failed to allocate bitwiseANDConst handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn  = TISP_toDsplibBufParams2D<T>(_in);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<T>(_out);
         DSPLIB_bufParams1D_t bufParamsConstant;
         bufParamsConstant.data_type = static_cast<uint32_t>(DSPLIB_element_dType<T>::value);
         bufParamsConstant.dim_x     = 1U;

         DSPLIB_STATUS status = DSPLIB_bitwiseANDConst_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsConstant,
                                                                        &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_bitwiseANDConst_init_checkParams")) {
            status = DSPLIB_bitwiseANDConst_init(_handle.get(), &bufParamsIn, &bufParamsConstant, &bufParamsOut,
                                                 &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_bitwiseANDConst_init");
         }
      }
   }
}

template <typename T> void *BitwiseANDConst<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t BitwiseANDConst<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *BitwiseANDConst<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t BitwiseANDConst<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void BitwiseANDConst<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void BitwiseANDConst<T>::exec() noexcept
{
   DSPLIB_bitwiseANDConst_exec(_handle.get(), _in.pBuf, &_constant, _out.pBuf);
}

template <typename T>
bool BitwiseANDConst<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("BitwiseANDConst::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<BitwiseANDConstParamId>(paramId)) {
      case BitwiseANDConstParamId::kConstant: {
         if (size < sizeof(T)) {
            errorCtx.setInvalidParameterError("BitwiseANDConst::setParams",
                                              "Value size smaller than expected for kConstant");
         }
         else {
            memcpy(&_constant, value, sizeof(T));
         }
         break;
      }
      case BitwiseANDConstParamId::kAll: {
         const BitwiseANDConstTunableParams<T> *pParams = static_cast<const BitwiseANDConstTunableParams<T> *>(value);
         if (size < sizeof(BitwiseANDConstTunableParams<T>)) {
            errorCtx.setInvalidParameterError("BitwiseANDConst::setParams",
                                              "Value size smaller than expected for kAll");
         }
         else {
            _constant = pParams->constant;
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("BitwiseANDConst::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool BitwiseANDConst<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("BitwiseANDConst::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("BitwiseANDConst::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<BitwiseANDConstParamId>(paramId)) {
      case BitwiseANDConstParamId::kConstant: {
         if (*size < sizeof(T)) {
            errorCtx.setInvalidParameterError("BitwiseANDConst::getParams", "Value size smaller than expected");
         }
         else {
            memcpy(value, &_constant, sizeof(T));
         }
         *size = sizeof(T);
         break;
      }
      case BitwiseANDConstParamId::kAll: {
         BitwiseANDConstTunableParams<T> *pParams = static_cast<BitwiseANDConstTunableParams<T> *>(value);
         if (*size < sizeof(BitwiseANDConstTunableParams<T>)) {
            errorCtx.setInvalidParameterError("BitwiseANDConst::getParams", "Value size smaller than expected");
         }
         else {
            pParams->constant = _constant;
         }
         *size = sizeof(BitwiseANDConstTunableParams<T>);
         break;
      }
      default:
         errorCtx.setInvalidParameterError("BitwiseANDConst::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template class BitwiseANDConst<uint8_t>;
template class BitwiseANDConst<uint16_t>;
template class BitwiseANDConst<uint32_t>;
template class BitwiseANDConst<uint64_t>;

} // namespace DSPLIB
} // namespace TISP
