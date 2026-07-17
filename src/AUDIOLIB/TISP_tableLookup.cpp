// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_tableLookup.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Tablelookup constructor]

template <typename T>
Tablelookup<T>::Tablelookup(T         *pX,
                            T         *pG,
                            T         *pY,
                            size_t     srcSamples,
                            size_t     tableSamples,
                            float      minVal,
                            float      maxVal,
                            uint32_t   tableLookupSize,
                            ErrorCtxt &errorCtx)
    : _pX(pX), _pG(pG), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "srcSamples=%zu,tableSamples=%zu,minVal=%.2f,maxVal=%.2f", srcSamples, tableSamples, minVal, maxVal);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Tablelookup constructor", "input pointer pX is null");
   }
   if (pG == nullptr) {
      errorCtx.setNullPointerError("Tablelookup constructor", "tableLookup pointer pG is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Tablelookup constructor", "output pointer pY is null");
   }
   if (srcSamples == 0) {
      errorCtx.setInvalidParameterError("Tablelookup constructor", "srcSamples must be greater than zero");
   }
   if (tableSamples == 0) {
      errorCtx.setInvalidParameterError("Tablelookup constructor", "tableSamples must be greater than zero");
   }
   if (tableLookupSize == 0) {
      errorCtx.setInvalidParameterError("Tablelookup constructor", "tableLookupSize must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _srcSamples      = srcSamples;
      _tableSamples    = tableSamples;
      _minVal          = minVal;
      _maxVal          = maxVal;
      _tableLookupSize = tableLookupSize;

      // Initialize bufParams with node input and output buffer parameters
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
      /* Initialize data structures required by AUDIOLIB_Tablelookup from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_tableLookup_InitArgs kerInitArgs;
      kerInitArgs.funcStyle       = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.minVal          = minVal;
      kerInitArgs.maxVal          = maxVal;
      kerInitArgs.tableLookupSize = tableLookupSize;

      _handle = make_unique_handle<char>(AUDIOLIB_tableLookup_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Tablelookup constructor", "failed to allocate tableLookup handle");
      }

      if (errorCtx.isSuccess()) {

         AUDIOLIB_bufParams1D_t bufParamsIn;
         AUDIOLIB_bufParams1D_t bufParamsOut;
         AUDIOLIB_bufParams1D_t bufParamsTablelookup;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(srcSamples);

         bufParamsTablelookup.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsTablelookup.dim_x     = static_cast<uint32_t>(tableSamples);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(srcSamples);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet = AUDIOLIB_tableLookup_init_checkParams(
             _handle.get(), &bufParamsIn, &bufParamsTablelookup, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_tableLookup_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for tableLookup to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_tableLookup_init(_handle.get(), &bufParamsIn, &bufParamsTablelookup,
                                                                &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_tableLookup_init");
#if defined(__C7524__)
            if (tableLookupSize >= tableSamples) {
               if (errorCtx.isSuccess()) {
                  AUDIOLIB_tableLookup_set_ci(_handle.get(), _pG);
                  // errorCtx.remapXLIBError(setRet, "AUDIOLIB_tableLookup_set_ci");
               }
            }
#endif
         }
      }
   }
}

template <typename T>
Tablelookup<T>::Tablelookup(T          *pX,
                            T          *pG,
                            T          *pY,
                            size_t      srcSamples,
                            size_t      tableSamples,
                            float       minVal,
                            float       maxVal,
                            uint32_t    tableLookupSize,
                            const char *nodeName,
                            uint32_t    nodeId,
                            ErrorCtxt  &errorCtx)
    : Tablelookup(pX, pG, pY, srcSamples, tableSamples, minVal, maxVal, tableLookupSize, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Tablelookup<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Tablelookup<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Tablelookup<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Tablelookup<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Tablelookup setAddr]
template <typename T> void Tablelookup<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Tablelookup<T>::exec() noexcept { AUDIOLIB_tableLookup_exec(_handle.get(), _pX, _pG, _pY); }

template class Tablelookup<float>;

} // namespace AUDIOLIB
} // namespace TISP
