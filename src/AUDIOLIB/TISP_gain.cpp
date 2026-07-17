// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_gain.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Gain constructor]

template <typename T>
Gain<T>::Gain(T         *pX,
              T         *pG,
              T         *pY,
              size_t     width,
              size_t     height,
              size_t     inPitchBytes,
              size_t     outPitchBytes,
              ErrorCtxt &errorCtx)
    : _pX(pX), _pG(pG), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", width, height);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Gain constructor", "input pointer pX is null");
   }
   if (pG == nullptr) {
      errorCtx.setNullPointerError("Gain constructor", "gain pointer pG is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Gain constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Gain constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("Gain constructor", "height must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width         = width;
      _height        = height;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;

      // Initialize bufParams with node input and output buffer parameters.
      // Gain is a pointwise operation with no interleaving notion, so the
      // interleave flags are left non-interleaved (0).
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
      /* Initialize data structures required by AUDIOLIB_Gain from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_gain_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(AUDIOLIB_gain_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Gain constructor", "failed to allocate gain handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;
         AUDIOLIB_bufParams1D_t bufParamsGain;

         bufParamsIn.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsGain.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsGain.dim_x     = static_cast<uint32_t>(1);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_gain_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsGain, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_gain_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for gain to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet =
                AUDIOLIB_gain_init(_handle.get(), &bufParamsIn, &bufParamsGain, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_gain_init");
         }
      }
   }
}

template <typename T>
Gain<T>::Gain(T          *pX,
              T          *pG,
              T          *pY,
              size_t      width,
              size_t      height,
              size_t      inPitchBytes,
              size_t      outPitchBytes,
              const char *nodeName,
              uint32_t    nodeId,
              ErrorCtxt  &errorCtx)
    : Gain(pX, pG, pY, width, height, inPitchBytes, outPitchBytes, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Gain<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Gain<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Gain<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Gain<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Gain setAddr]
template <typename T> void Gain<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Gain<T>::exec() noexcept { AUDIOLIB_gain_exec(_handle.get(), _pX, _pG, _pY); }

template class Gain<float>;
template class Gain<double>;

} // namespace AUDIOLIB
} // namespace TISP
