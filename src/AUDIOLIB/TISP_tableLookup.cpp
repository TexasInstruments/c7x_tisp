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

template <typename TIn, typename TOut>
Tablelookup<TIn, TOut>::Tablelookup(const TISP_BufDescList               *pInputs,
                                    const TISP_BufDescList               *pOutputs,
                                    const TablelookupConfig              *pConfig,
                                    const TablelookupTunableParams<TOut> *pDefaultTunableParams,
                                    const char                           *nodeName,
                                    uint32_t                              nodeId,
                                    ErrorCtxt                            &errorCtx)
    : _in{}, _out{}, _table(nullptr)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("Tablelookup constructor", "build params pointer pConfig is null");
   }
   else if (pDefaultTunableParams == nullptr) {
      errorCtx.setNullPointerError("Tablelookup constructor", "default params pointer pDefaultTunableParams is null");
   }
   else if (pDefaultTunableParams->pTable == nullptr) {
      errorCtx.setNullPointerError("Tablelookup constructor", "table pointer pDefaultTunableParams->pTable is null");
   }
   else {
      _table = pDefaultTunableParams->pTable;
   }

   // Buffer checks (skipped once an error is set): one input of TIn, one output of TOut
   if (TISP_checkBufDescList<TIn>(pInputs, 1U, 1U, "Tablelookup constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<TOut>(pOutputs, 1U, 1U, "Tablelookup constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if ((_out.width != _in.width) || (_out.height != _in.height)) {
         errorCtx.setInvalidParameterError("Tablelookup constructor", "output width and height must match the input");
      }
      else if ((_in.height > 1U) &&
               ((_in.pitchBytes != (_in.width * sizeof(TIn))) || (_out.pitchBytes != (_out.width * sizeof(TOut))))) {
         errorCtx.setInvalidParameterError("Tablelookup constructor", "input and output rows must be packed");
      }
      else if (pConfig->tableSamples == 0U) {
         errorCtx.setInvalidParameterError("Tablelookup constructor", "tableSamples must be greater than zero");
      }
      else if (pConfig->tableLookupSize == 0U) {
         errorCtx.setInvalidParameterError("Tablelookup constructor", "tableLookupSize must be greater than zero");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      const size_t numSamples = _in.width * _in.height;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "numSamples=%zu, tableSamples=%zu, minVal=%.2f, maxVal=%.2f", numSamples, pConfig->tableSamples,
                      static_cast<double>(pConfig->minVal), static_cast<double>(pConfig->maxVal));
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_tableLookup        */
      /***********************************************************************/
      AUDIOLIB_tableLookup_InitArgs kerInitArgs;
      kerInitArgs.funcStyle       = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.minVal          = pConfig->minVal;
      kerInitArgs.maxVal          = pConfig->maxVal;
      kerInitArgs.tableLookupSize = pConfig->tableLookupSize;

      _handle = make_unique_handle<char>(AUDIOLIB_tableLookup_getHandleSize(&kerInitArgs));
      if (!_handle) {
         errorCtx.setMemoryError("Tablelookup constructor", "failed to allocate tableLookup handle");
      }
      else {
         AUDIOLIB_bufParams1D_t bufParamsIn;
         AUDIOLIB_bufParams1D_t bufParamsTable;
         AUDIOLIB_bufParams1D_t bufParamsOut;

         bufParamsIn.data_type    = static_cast<uint32_t>(AUDIOLIB_element_dType<TIn>::value);
         bufParamsIn.dim_x        = static_cast<uint32_t>(numSamples);
         bufParamsTable.data_type = static_cast<uint32_t>(AUDIOLIB_element_dType<TOut>::value);
         bufParamsTable.dim_x     = static_cast<uint32_t>(pConfig->tableSamples);
         bufParamsOut.data_type   = static_cast<uint32_t>(AUDIOLIB_element_dType<TOut>::value);
         bufParamsOut.dim_x       = static_cast<uint32_t>(numSamples);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/
         AUDIOLIB_STATUS status = AUDIOLIB_tableLookup_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsTable,
                                                                        &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from AUDIOLIB for tableLookup to set SE/SA params       */
         /******************************************************************************/
         if (errorCtx.remapXLIBError(status, "AUDIOLIB_tableLookup_init_checkParams")) {
            status =
                AUDIOLIB_tableLookup_init(_handle.get(), &bufParamsIn, &bufParamsTable, &bufParamsOut, &kerInitArgs);
            if (errorCtx.remapXLIBError(status, "AUDIOLIB_tableLookup_init")) {
#if defined(__C7524__)
               // A table that fits the hardware lookup table is loaded into it here; exec then reads it from there
               if (pConfig->tableSamples <= pConfig->tableLookupSize) {
                  AUDIOLIB_tableLookup_set_ci(_handle.get(), _table);
               }
#endif
            }
         }
      }
   }
}

template <typename TIn, typename TOut> void *Tablelookup<TIn, TOut>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename TIn, typename TOut> void *Tablelookup<TIn, TOut>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename TIn, typename TOut> size_t Tablelookup<TIn, TOut>::getNumInputs() const noexcept { return 1; }

template <typename TIn, typename TOut> size_t Tablelookup<TIn, TOut>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Tablelookup setAddr]
template <typename TIn, typename TOut>
void Tablelookup<TIn, TOut>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename TIn, typename TOut> void Tablelookup<TIn, TOut>::exec() noexcept
{
   AUDIOLIB_tableLookup_exec(_handle.get(), _in.pBuf, _table, _out.pBuf);
}

template class Tablelookup<float, float>;
template class Tablelookup<int32_t, float>;
template class Tablelookup<int32_t, int32_t>;

} // namespace AUDIOLIB
} // namespace TISP
