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
TypeConversion<Tin, Tout>::TypeConversion(Tin       *pX,
                                          Tout      *pY,
                                          size_t     width,
                                          size_t     height,
                                          size_t     inPitchBytes,
                                          size_t     outPitchBytes,
                                          size_t     testQ31,
                                          ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu,testQ31=%zu", width, height,
                   testQ31);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("TypeConversion constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("TypeConversion constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("TypeConversion constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("TypeConversion constructor", "height must be greater than zero");
   }
   if (testQ31 > 1) {
      errorCtx.setInvalidParameterError("TypeConversion constructor", "testQ31 must be either zero or one");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width         = width;
      _height        = height;
      _inPitchBytes  = inPitchBytes;
      _outPitchBytes = outPitchBytes;
      _testQ31       = testQ31;

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<Tin>::value;
      _nodeInfo.bufParams.isInputInterleave = 1;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = inPitchBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<Tout>::value;
      _nodeInfo.bufParams.isOutputInterleave = 1;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Typeconversion from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_typeConversion_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.testQ31   = testQ31;

      _handle = make_unique_handle<char>(AUDIOLIB_typeConversion_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("TypeConversion constructor", "failed to allocate typeConversion handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams2D_t bufParamsIn;
         AUDIOLIB_bufParams2D_t bufParamsOut;

         bufParamsIn.data_type = AUDIOLIB_element_dType<Tin>::value;
         bufParamsIn.dim_x     = static_cast<uint32_t>(width);
         bufParamsIn.dim_y     = static_cast<uint32_t>(height);
         bufParamsIn.stride_y  = static_cast<int32_t>(inPitchBytes);

         bufParamsOut.data_type = AUDIOLIB_element_dType<Tout>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_typeConversion_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_typeConversion_init_checkParams");
         /******************************************************************************/
         /* Call init function from AUDIOLIB to for typeConversion to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet =
                AUDIOLIB_typeConversion_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_typeConversion_init");
         }
      }
   }
}

//! [TISP::Typeconversion setAddr]
template <typename Tin, typename Tout> void *TypeConversion<Tin, Tout>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename Tin, typename Tout> void *TypeConversion<Tin, Tout>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename Tin, typename Tout> size_t TypeConversion<Tin, Tout>::getNumInputs() const noexcept { return 1; }

template <typename Tin, typename Tout> size_t TypeConversion<Tin, Tout>::getNumOutputs() const noexcept { return 1; }

template <typename Tin, typename Tout>
void TypeConversion<Tin, Tout>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<Tin *>(addr.src[0]);
   }
   if (mode == kAddrModeOut) {
      _pY = static_cast<Tout *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<Tin *>(addr.src[0]);
      _pY = static_cast<Tout *>(addr.dst[0]);
   }
}

template <typename Tin, typename Tout> void TypeConversion<Tin, Tout>::exec() noexcept
{

   AUDIOLIB_typeConversion_exec(_handle.get(), _pX, _pY);
}

template <typename Tin, typename Tout>
TypeConversion<Tin, Tout>::TypeConversion(Tin        *pX,
                                          Tout       *pY,
                                          size_t      width,
                                          size_t      height,
                                          size_t      inPitchBytes,
                                          size_t      outPitchBytes,
                                          size_t      testQ31,
                                          const char *nodeName,
                                          uint32_t    nodeId,
                                          ErrorCtxt  &errorCtx)
    : TypeConversion(pX, pY, width, height, inPitchBytes, outPitchBytes, testQ31, errorCtx)
{
   (void) pX;
   (void) pY;
   (void) width;
   (void) height;
   (void) inPitchBytes;
   (void) outPitchBytes;
   (void) testQ31;
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
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
