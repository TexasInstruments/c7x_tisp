// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_whiteNoise.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Whitenoise constructor]

template <typename T>
Whitenoise<T>::Whitenoise(T *pY, size_t width, float range, uint32_t seed, ErrorCtxt &errorCtx) : _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,range=%.2f,seed=%u", width, range,
                   seed);

   // Input validation checks
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Whitenoise constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Whitenoise constructor", "width must be greater than zero");
   }
   if (range == 0) {
      errorCtx.setInvalidParameterError("Whitenoise constructor", "range must be greater than zero");
   }
   if (seed == 0) {
      errorCtx.setInvalidParameterError("Whitenoise constructor", "seed must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width = width;
      _range = range;
      _seed  = seed;

      // Initialize bufParams with node input and output buffer parameters (source node: no input buffer)
      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = 1;
      _nodeInfo.bufParams.outPitchBytes[0]   = width * sizeof(T);
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Whitenoise from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_whiteNoiseGenerator_InitArgs kerInitArgs;
      kerInitArgs.funcStyle = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.seed      = seed;
      kerInitArgs.range     = range;

      _handle = make_unique_handle<char>(AUDIOLIB_whiteNoiseGenerator_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Whitenoise constructor", "failed to allocate whiteNoiseGenerator handle");
      }

      if (errorCtx.isSuccess()) {
         AUDIOLIB_bufParams1D_t bufParamsOut;

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_whiteNoiseGenerator_init_checkParams(_handle.get(), &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_whiteNoiseGenerator_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for whiteNoiseGenerator to set SE/SA params             */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_whiteNoiseGenerator_init(_handle.get(), &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_whiteNoiseGenerator_init");
         }
      }
   }
}

template <typename T>
Whitenoise<T>::Whitenoise(T          *pY,
                          size_t      width,
                          float       range,
                          uint32_t    seed,
                          const char *nodeName,
                          uint32_t    nodeId,
                          ErrorCtxt  &errorCtx)
    : Whitenoise(pY, width, range, seed, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Whitenoise<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Whitenoise<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Whitenoise setAddr]
template <typename T> void Whitenoise<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void Whitenoise<T>::exec() noexcept
{
   AUDIOLIB_whiteNoiseGenerator_exec(_handle.get(), static_cast<void *>(_pY));
}

template class Whitenoise<float>;

} // namespace AUDIOLIB
} // namespace TISP
