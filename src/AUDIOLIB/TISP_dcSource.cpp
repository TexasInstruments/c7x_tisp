// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_dcSource.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Dcsource constructor]

template <typename T>
Dcsource<T>::Dcsource(T *pC, T *pY, size_t width, size_t height, size_t outPitchBytes, ErrorCtxt &errorCtx)
    : _pC(pC), _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", width, height);

   // Input validation checks
   if (pC == nullptr) {
      errorCtx.setNullPointerError("Dcsource constructor", "constant input pointer pC is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Dcsource constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Dcsource constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("Dcsource constructor", "height must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width         = width;
      _height        = height;
      _outPitchBytes = outPitchBytes;

      // Initialize bufParams with node output buffer parameters. This node is a pure
      // generator (pC is a scalar constant, not a data buffer), so there is no input side.
      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = outPitchBytes;
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Dcsource from AUDIOLIB */
      /***********************************************************************/

      DSPLIB_blkCopyConst2DInitArgs kerInitArgs;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_blkCopyConst2D_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Dcsource constructor", "failed to allocate dcSource handle");
      }

      if (errorCtx.isSuccess()) {
         DSPLIB_bufParams2D_t bufParamsOut;
         DSPLIB_bufParams1D_t bufParamsIn1;

         bufParamsIn1.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsIn1.dim_x     = static_cast<uint32_t>(1);

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);
         bufParamsOut.dim_y     = static_cast<uint32_t>(height);
         bufParamsOut.stride_y  = static_cast<int32_t>(outPitchBytes);
         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         DSPLIB_STATUS checkRet =
             DSPLIB_blkCopyConst2D_init_checkParams(_handle.get(), &bufParamsIn1, &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "DSPLIB_blockCopyConst2D_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for dcSource to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            DSPLIB_STATUS initRet =
                DSPLIB_blkCopyConst2D_init(_handle.get(), &bufParamsIn1, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "DSPLIB_blockCopyConst2D_init");
         }
      }
   }
}

template <typename T>
Dcsource<T>::Dcsource(T          *pC,
                      T          *pY,
                      size_t      width,
                      size_t      height,
                      size_t      outPitchBytes,
                      const char *nodeName,
                      uint32_t    nodeId,
                      ErrorCtxt  &errorCtx)
    : Dcsource(pC, pY, width, height, outPitchBytes, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Dcsource<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Dcsource<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Dcsource setAddr]
template <typename T> void Dcsource<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void Dcsource<T>::exec() noexcept { DSPLIB_blkCopyConst2D_exec(_handle.get(), _pC, _pY); }

template class Dcsource<float>;
template class Dcsource<double>;
template class Dcsource<int8_t>;
template class Dcsource<uint8_t>;
template class Dcsource<int16_t>;
template class Dcsource<uint16_t>;
template class Dcsource<int32_t>;
template class Dcsource<uint32_t>;
template class Dcsource<int64_t>;
template class Dcsource<uint64_t>;

} // namespace AUDIOLIB
} // namespace TISP
