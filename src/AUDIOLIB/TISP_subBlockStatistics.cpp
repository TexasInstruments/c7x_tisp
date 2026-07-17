// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_subBlockStatistics.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Subblockstatistics constructor]

template <typename T>
SubBlockStatistics<T>::SubBlockStatistics(T         *pX,
                                          T         *pY,
                                          size_t     widthIn,
                                          size_t     heightIn,
                                          size_t     inPitchBytes,
                                          size_t     outPitchBytes,
                                          size_t     subBlockSize,
                                          size_t     flagPerChanStats,
                                          size_t     statisticsType,
                                          ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", widthIn, heightIn);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("SubBlockStatistics constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("SubBlockStatistics constructor", "output pointer pY is null");
   }
   if (widthIn == 0) {
      errorCtx.setInvalidParameterError("SubBlockStatistics constructor", "widthIn must be greater than zero");
   }
   if (heightIn == 0) {
      errorCtx.setInvalidParameterError("SubBlockStatistics constructor", "heightIn must be greater than zero");
   }
   if (subBlockSize == 0) {
      errorCtx.setInvalidParameterError("SubBlockStatistics constructor", "subBlockSize must be greater than zero");
   }
   if (flagPerChanStats > 1) {
      errorCtx.setInvalidParameterError("SubBlockStatistics constructor",
                                        "flagPerChanStats must be either zero or one");
   }
   if (statisticsType > 9) {
      errorCtx.setInvalidParameterError("SubBlockStatistics constructor",
                                        "statisticsType must be between zero and nine");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _widthIn          = widthIn;
      _heightIn         = heightIn;
      _inPitchBytes     = inPitchBytes;
      _outPitchBytes    = outPitchBytes;
      _subBlockSize     = subBlockSize;
      _flagPerChanStats = flagPerChanStats;
      _statisticsType   = statisticsType;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Subblockstatistics from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_subBlockStatistics_InitArgs kerInitArgs;
      kerInitArgs.funcStyle    = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.subBlockSize = subBlockSize;

      AUDIOLIB_subBlockStatistics_SetArgs kerSetArgs;
      kerSetArgs.flagPerChanStats = flagPerChanStats;
      kerSetArgs.statisticsType   = statisticsType;
      kerSetArgs.funcStyle        = AUDIOLIB_FUNCTION_OPTIMIZED;

      size_t heightOut;
      size_t widthOut = widthIn / subBlockSize;
      if (flagPerChanStats == 0) {
         heightOut = 1;
      }
      else {
         heightOut = heightIn;
      }
      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = widthIn;
      _nodeInfo.bufParams.heightIn[0]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = widthOut;
      _nodeInfo.bufParams.heightOut[0]       = heightOut;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;

      _handle = make_unique_handle<char>(AUDIOLIB_subBlockStatistics_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("SubBlockStatistics constructor", "failed to allocate subBlockStatistics handle");
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

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_subBlockStatistics_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_subBlockStatistics_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for subBlockStatistics to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet =
                AUDIOLIB_subBlockStatistics_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_subBlockStatistics_init");
            if (errorCtx.isSuccess()) {
               AUDIOLIB_STATUS setRet = AUDIOLIB_subBlockStatistics_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(setRet, "AUDIOLIB_subBlockStatistics_set");
            }
         }
      }
   }
}

template <typename T> void *SubBlockStatistics<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *SubBlockStatistics<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t SubBlockStatistics<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t SubBlockStatistics<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::SubBlockStatistics setAddr]
template <typename T> void SubBlockStatistics<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void SubBlockStatistics<T>::exec() noexcept
{
   AUDIOLIB_subBlockStatistics_exec(_handle.get(), _pX, _pY);
}

template <typename T>
SubBlockStatistics<T>::SubBlockStatistics(T          *pX,
                                          T          *pY,
                                          size_t      widthIn,
                                          size_t      heightIn,
                                          size_t      inPitchBytes,
                                          size_t      outPitchBytes,
                                          size_t      subBlockSize,
                                          size_t      flagPerChanStats,
                                          size_t      statisticsType,
                                          const char *nodeName,
                                          uint32_t    nodeId,
                                          ErrorCtxt  &errorCtx)
    : SubBlockStatistics(pX,
                         pY,
                         widthIn,
                         heightIn,
                         inPitchBytes,
                         outPitchBytes,
                         subBlockSize,
                         flagPerChanStats,
                         statisticsType,
                         errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class SubBlockStatistics<float>;
template class SubBlockStatistics<double>;

} // namespace AUDIOLIB
} // namespace TISP
