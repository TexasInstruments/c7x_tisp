// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_recip.hpp>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace TISP {
namespace DSPLIB {

template <typename T>
Recip<T>::Recip(const TISP_BufDescList *pInputs,
                const TISP_BufDescList *pOutputs,
                const RecipConfig      *pConfig,
                const char             *nodeName,
                uint32_t                nodeId,
                ErrorCtxt              &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("Recip constructor", "build params pointer pConfig is null");
   }
   else {
      /* Nothing to do here */
   }

   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "Recip constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "Recip constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_out.isInterleave != _in.isInterleave) {
         errorCtx.setInvalidParameterError("Recip constructor", "output 0 layout must match input 0");
      }
      else if (std::is_same<T, int16_t>::value ? ((pConfig->storeStyle != 1U) && (pConfig->storeStyle != 2U))
                                               : (pConfig->storeStyle != 0U)) {
         errorCtx.setInvalidParameterError("Recip constructor",
                                           "storeStyle must be 1 (HSTACK) or 2 (INTERLEAVE) for int16 and 0 (LINEAR) "
                                           "for float and double");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,isInterleave=%u",
                      _in.width, _in.height, static_cast<uint32_t>(_in.isInterleave));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      DSPLIB_recip_InitArgs kerInitArgs;
      kerInitArgs.funcStyle  = DSPLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.dataSize   = static_cast<uint32_t>(_in.width);
      kerInitArgs.storeStyle = static_cast<DSPLIB_recip_storeStyle>(pConfig->storeStyle);

      _handle = make_unique_handle<char>(DSPLIB_recip_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("Recip constructor", "failed to allocate recip handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn  = TISP_toDsplibBufParams2D<T>(_in);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<T>(_out);

         DSPLIB_STATUS status = DSPLIB_recip_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         if (errorCtx.remapXLIBError(status, "DSPLIB_recip_init_checkParams")) {
            status = DSPLIB_recip_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_recip_init");
         }
      }
   }
}

template <typename T> void *Recip<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> size_t Recip<T>::getNumInputs() const noexcept { return 1; }

template <typename T> void *Recip<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t Recip<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Recip<T>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addrs.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addrs.dst[0];
   }
}

template <typename T> void Recip<T>::exec() noexcept { DSPLIB_recip_exec(_handle.get(), _in.pBuf, _out.pBuf); }

template class Recip<int16_t>;
template class Recip<float>;
template class Recip<double>;

} // namespace DSPLIB
} // namespace TISP
