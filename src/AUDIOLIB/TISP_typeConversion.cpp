// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_typeConversion.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::TypeConversion constructor]

template <typename Tin, typename Tout>
TypeConversion<Tin, Tout>::TypeConversion(const TISP_BufDescList          *pInputs,
                                          const TISP_BufDescList          *pOutputs,
                                          const TypeConversionConfig *pConfig,
                                          const char                      *nodeName,
                                          uint32_t                         nodeId,
                                          ErrorCtxt                       &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("TypeConversion constructor", "build params pointer pConfig is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one Tin input, one Tout output
   if (TISP_checkBufDescList<Tin>(pInputs, 1U, 1U, "TypeConversion constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<Tout>(pOutputs, 1U, 1U, "TypeConversion constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if (_in.isInterleave != _out.isInterleave) {
         errorCtx.setInvalidParameterError("TypeConversion constructor", "input and output interleave must match");
      }
      else if (pConfig->testQ31 > 1U) {
         errorCtx.setInvalidParameterError("TypeConversion constructor", "testQ31 must be either zero or one");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,testQ31=%u", _in.width,
                      _in.height, static_cast<uint32_t>(pConfig->testQ31));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Typeconversion from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_typeConversion_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.testQ31   = pConfig->testQ31;

      _handle = make_unique_handle<char>(AUDIOLIB_typeConversion_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("TypeConversion constructor", "failed to allocate typeConversion handle");
      }
      else {
         AUDIOLIB_bufParams2D_t bufParamsIn  = TISP_toAudiolibBufParams2D<Tin>(_in);
         AUDIOLIB_bufParams2D_t bufParamsOut = TISP_toAudiolibBufParams2D<Tout>(_out);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS status =
             AUDIOLIB_typeConversion_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for typeConversion to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "AUDIOLIB_typeConversion_init_checkParams")) {
            status = AUDIOLIB_typeConversion_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "AUDIOLIB_typeConversion_init");
         }
      }
   }
}

template <typename Tin, typename Tout> void *TypeConversion<Tin, Tout>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename Tin, typename Tout> void *TypeConversion<Tin, Tout>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename Tin, typename Tout> size_t TypeConversion<Tin, Tout>::getNumInputs() const noexcept { return 1; }

template <typename Tin, typename Tout> size_t TypeConversion<Tin, Tout>::getNumOutputs() const noexcept { return 1; }

//! [TISP::TypeConversion setAddr]
template <typename Tin, typename Tout>
void TypeConversion<Tin, Tout>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _in.pBuf = addr.src[0];
   }
   if (mode == kAddrModeOut) {
      _out.pBuf = addr.dst[0];
   }

   if (mode == kAddrModeInOut) {
      _in.pBuf  = addr.src[0];
      _out.pBuf = addr.dst[0];
   }
}

template <typename Tin, typename Tout> void TypeConversion<Tin, Tout>::exec() noexcept
{
   AUDIOLIB_typeConversion_exec(_handle.get(), _in.pBuf, _out.pBuf);
}

template class TypeConversion<float, int16_t>;
template class TypeConversion<double, int16_t>;
template class TypeConversion<int16_t, float>;
template class TypeConversion<int16_t, double>;
template class TypeConversion<float, int32_t>;
template class TypeConversion<double, int32_t>;
template class TypeConversion<int32_t, float>;
template class TypeConversion<int32_t, double>;

} // namespace AUDIOLIB
} // namespace TISP
