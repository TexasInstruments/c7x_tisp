// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_tableInterpolate.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Tableinterpolate constructor]

template <typename T>
Tableinterpolate<T>::Tableinterpolate(T         *pX,
                                      T         *pG,
                                      T         *pY,
                                      size_t     srcSamples,
                                      size_t     tableSamples,
                                      float      minVal,
                                      float      maxVal,
                                      uint32_t   tableInterpolateSize,
                                      ErrorCtxt &errorCtx)
    : _pX(pX), _pG(pG), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "srcSamples=%zu,tableSamples=%zu,minVal=%.2f,maxVal=%.2f", srcSamples, tableSamples, minVal, maxVal);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Tableinterpolate constructor", "input pointer pX is null");
   }
   if (pG == nullptr) {
      errorCtx.setNullPointerError("Tableinterpolate constructor", "table pointer pG is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Tableinterpolate constructor", "output pointer pY is null");
   }
   if (srcSamples == 0) {
      errorCtx.setInvalidParameterError("Tableinterpolate constructor", "srcSamples must be greater than zero");
   }
   if (tableSamples == 0) {
      errorCtx.setInvalidParameterError("Tableinterpolate constructor", "tableSamples must be greater than zero");
   }
   if (tableInterpolateSize == 0) {
      errorCtx.setInvalidParameterError("Tableinterpolate constructor",
                                        "tableInterpolateSize must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _srcSamples           = srcSamples;
      _tableSamples         = tableSamples;
      _minVal               = minVal;
      _maxVal               = maxVal;
      _tableInterpolateSize = tableInterpolateSize;

      // Initialize bufParams with node input and output buffer parameters. pG (lookup
      // table) is an internal operand, not exposed via getInputAddr/setAddr, so it does
      // not get a bufParams slot.
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = srcSamples;
      _nodeInfo.bufParams.heightIn[0]       = 1;
      _nodeInfo.bufParams.inPitchBytes[0]   = srcSamples * sizeof(T);

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = srcSamples;
      _nodeInfo.bufParams.heightOut[0]       = 1;
      _nodeInfo.bufParams.outPitchBytes[0]   = srcSamples * sizeof(T);
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Tableinterpolate from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_tableInterpolate_InitArgs kerInitArgs;
      kerInitArgs.funcStyle            = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.minVal               = minVal;
      kerInitArgs.maxVal               = maxVal;
      kerInitArgs.tableInterpolateSize = tableInterpolateSize;

      _handle = make_unique_handle<char>(AUDIOLIB_tableInterpolate_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Tableinterpolate constructor", "failed to allocate tableInterpolate handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams1D_t bufParamsIn;
         AUDIOLIB_bufParams1D_t bufParamsOut;
         AUDIOLIB_bufParams1D_t bufParamsTableinterpolate;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(srcSamples);

         bufParamsTableinterpolate.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsTableinterpolate.dim_x     = static_cast<uint32_t>(tableSamples);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(srcSamples);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet = AUDIOLIB_tableInterpolate_init_checkParams(
             _handle.get(), &bufParamsIn, &bufParamsTableinterpolate, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_tableInterpolate_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for tableInterpolate to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_tableInterpolate_init(
                _handle.get(), &bufParamsIn, &bufParamsTableinterpolate, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_tableInterpolate_init");
#if defined(__C7524__)
            if (tableInterpolateSize >= tableSamples) {
               if (errorCtx.isSuccess()) {
                  AUDIOLIB_tableInterpolate_set_ci(_handle.get(), _pG);
                  // errorCtx.remapXLIBError(setRet, "AUDIOLIB_tableLookup_set_ci");
               }
            }
#endif
         }
      }
   }
}

template <typename T>
Tableinterpolate<T>::Tableinterpolate(T          *pX,
                                      T          *pG,
                                      T          *pY,
                                      size_t      srcSamples,
                                      size_t      tableSamples,
                                      float       minVal,
                                      float       maxVal,
                                      uint32_t    tableInterpolateSize,
                                      const char *nodeName,
                                      uint32_t    nodeId,
                                      ErrorCtxt  &errorCtx)
    : Tableinterpolate(pX, pG, pY, srcSamples, tableSamples, minVal, maxVal, tableInterpolateSize, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Tableinterpolate<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Tableinterpolate<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Tableinterpolate<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Tableinterpolate<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Tableinterpolate setAddr]
template <typename T> void Tableinterpolate<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Tableinterpolate<T>::exec() noexcept
{
   AUDIOLIB_tableInterpolate_exec(_handle.get(), _pX, _pG, _pY);
}

template class Tableinterpolate<float>;

} // namespace AUDIOLIB
} // namespace TISP
