// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_blockStatistics.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Blockstatistics constructor]

template <typename T>
BlockStatistics<T>::BlockStatistics(T         *pX,
                                    T         *pY,
                                    size_t     widthIn,
                                    size_t     heightIn,
                                    size_t     inPitchBytes,
                                    size_t     outPitchBytes,
                                    size_t     statisticsType,
                                    ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", widthIn, heightIn);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("BlockStatistics constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("BlockStatistics constructor", "output pointer pY is null");
   }
   if (widthIn == 0) {
      errorCtx.setInvalidParameterError("BlockStatistics constructor", "width must be greater than zero");
   }
   if (heightIn == 0) {
      errorCtx.setInvalidParameterError("BlockStatistics constructor", "height must be greater than zero");
   }
   if (statisticsType > 9) {
      errorCtx.setInvalidParameterError("BlockStatistics constructor", "statisticsType must be between zero and nine");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _widthIn        = widthIn;
      _heightIn       = heightIn;
      _inPitchBytes   = inPitchBytes;
      _outPitchBytes  = outPitchBytes;
      _statisticsType = statisticsType;

      // Initialize bufParams with node input and output buffer parameters. The kernel reduces
      // each input channel to a single output value, so the output width is 1.
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = widthIn;
      _nodeInfo.bufParams.heightIn[0]       = heightIn;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = 1;
      _nodeInfo.bufParams.heightOut[0]       = heightIn;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Blockstatistics from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_blockStatistics_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;

      AUDIOLIB_blockStatistics_SetArgs kerSetArgs;
      kerSetArgs.statisticsType = statisticsType;
      kerSetArgs.funcStyle      = AUDIOLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(AUDIOLIB_blockStatistics_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("BlockStatistics constructor", "failed to allocate blockStatistics handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(widthIn);
         bufParamsIn.dim_y     = static_cast<uint32_t>(heightIn);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(1);
         bufParamsOut.dim_y     = static_cast<uint32_t>(heightIn);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_blockStatistics_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_blockStatistics_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for blockStatistics to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet =
                AUDIOLIB_blockStatistics_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_balanceFader_init");
            if (errorCtx.isSuccess()) {
               AUDIOLIB_STATUS setRet = AUDIOLIB_blockStatistics_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(setRet, "AUDIOLIB_blockStatistics_set");
            }
         }
      }
   }
}

template <typename T> void *BlockStatistics<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *BlockStatistics<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t BlockStatistics<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t BlockStatistics<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::BlockStatistics setAddr]
template <typename T> void BlockStatistics<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void BlockStatistics<T>::exec() noexcept
{
   AUDIOLIB_blockStatistics_exec(_handle.get(), _pX, _pY);
}

template <typename T>
BlockStatistics<T>::BlockStatistics(T          *pX,
                                    T          *pY,
                                    size_t      widthIn,
                                    size_t      heightIn,
                                    size_t      inPitchBytes,
                                    size_t      outPitchBytes,
                                    size_t      statisticsType,
                                    const char *nodeName,
                                    uint32_t    nodeId,
                                    ErrorCtxt  &errorCtx)
    : BlockStatistics(pX, pY, widthIn, heightIn, inPitchBytes, outPitchBytes, statisticsType, errorCtx)
{
   (void) pX;
   (void) pY;
   (void) widthIn;
   (void) heightIn;
   (void) inPitchBytes;
   (void) outPitchBytes;
   (void) statisticsType;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class BlockStatistics<float>;
template class BlockStatistics<double>;

} // namespace AUDIOLIB
} // namespace TISP
