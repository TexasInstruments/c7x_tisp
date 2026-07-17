// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_softClip.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Softclip constructor]

template <typename T>
Softclip<T>::Softclip(T         *pX,
                      T         *pY,
                      size_t     width,
                      size_t     height,
                      size_t     inPitchBytes,
                      size_t     outPitchBytes,
                      float      threshold,
                      float      endKnee,
                      ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,threshold=%f", width, height,
                   threshold);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Softclip constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Softclip constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Softclip constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("Softclip constructor", "height must be greater than zero");
   }
   if (threshold < 0) {
      errorCtx.setInvalidParameterError("Softclip constructor", "threshold must be greater than zero");
   }
   if (endKnee == 0) {
      errorCtx.setInvalidParameterError("Softclip constructor", "endKnee must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width         = width;
      _height        = height;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _threshold     = threshold;
      _endKnee       = endKnee;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Softclip from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_softClip_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;

      AUDIOLIB_softClip_SetArgs kerSetArgs;
      kerSetArgs.threshold = threshold;
      kerSetArgs.endKnee   = endKnee;

      _handle = make_unique_handle<char>(AUDIOLIB_softClip_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Softclip constructor", "failed to allocate softClip handle");
      }

      if (errorCtx.isSuccess()) {

         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_softClip_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_softClip_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for softClip to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_softClip_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_softClip_init");
            if (errorCtx.isSuccess()) {
               AUDIOLIB_STATUS setRet = AUDIOLIB_softClip_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(setRet, "AUDIOLIB_softClip_set");
            }
         }
      }
   }
}

template <typename T> void *Softclip<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Softclip<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Softclip<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Softclip<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Softclip setAddr]
template <typename T> void Softclip<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Softclip<T>::exec() noexcept { AUDIOLIB_softClip_exec(_handle.get(), _pX, _pY); }

template <typename T>
Softclip<T>::Softclip(T          *pX,
                      T          *pY,
                      size_t      width,
                      size_t      height,
                      size_t      inPitchBytes,
                      size_t      outPitchBytes,
                      float       threshold,
                      float       endKnee,
                      const char *nodeName,
                      uint32_t    nodeId,
                      ErrorCtxt  &errorCtx)
    : Softclip(pX, pY, width, height, inPitchBytes, outPitchBytes, threshold, endKnee, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class Softclip<float>;

} // namespace AUDIOLIB
} // namespace TISP
