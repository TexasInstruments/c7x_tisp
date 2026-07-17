// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_sinusoidGenerator.hpp>
#include <cstring>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Sinusoidgenerator constructor]

template <typename T>
Sinusoidgenerator<T>::Sinusoidgenerator(T         *pY,
                                        size_t     width,
                                        float      startPhase,
                                        float      smoothingTime,
                                        int32_t    samplingRate,
                                        uint32_t   executionMode,
                                        float      startFrequency,
                                        float      targetFrequency,
                                        uint32_t   dataType,
                                        ErrorCtxt &errorCtx)
    : _pY(pY)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "width=%zu,startFreq=%.2f,targetFreq=%.2f,samplingRate=%d", width, startFrequency, targetFrequency,
                   samplingRate);

   // Input validation checks
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Sinusoidgenerator constructor", "output pointer pY is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("Sinusoidgenerator constructor", "width must be greater than zero");
   }
   if (startPhase == 0) {
      errorCtx.setInvalidParameterError("Sinusoidgenerator constructor", "startPhase must be greater than zero");
   }
   if (smoothingTime < 0) {
      errorCtx.setInvalidParameterError("Sinusoidgenerator constructor", "smoothingTime must be greater than zero");
   }
   if (samplingRate == 0) {
      errorCtx.setInvalidParameterError("Sinusoidgenerator constructor", "samplingRate must be greater than zero");
   }
   if (executionMode > 1) {
      errorCtx.setInvalidParameterError("Sinusoidgenerator constructor", "executionMode must be either zero or one");
   }
   if (startFrequency == 0) {
      errorCtx.setInvalidParameterError("Sinusoidgenerator constructor", "startFrequency must be greater than zero");
   }
   if (targetFrequency == 0) {
      errorCtx.setInvalidParameterError("Sinusoidgenerator constructor", "targetFrequency must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _width           = width;
      _startPhase      = startPhase;
      _smoothingTime   = smoothingTime;
      _samplingRate    = samplingRate;
      _executionMode   = executionMode;
      _startFrequency  = startFrequency;
      _targetFrequency = targetFrequency;
      _dataType        = dataType;

      // Initialize bufParams with node input and output buffer parameters.
      // Sinusoidgenerator is a pure signal generator (no data input); only the output side is populated.
      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = 1;
      _nodeInfo.bufParams.outPitchBytes[0]   = width * sizeof(T);
      /***********************************************************************/
      /* Initialize data structures required by AUDIOLIB_Sinusoidgenerator from AUDIOLIB */
      /***********************************************************************/

      AUDIOLIB_sinusoidGenerator_InitArgs kerInitArgs;
      kerInitArgs.funcStyle     = AUDIOLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.executionMode = executionMode;

      AUDIOLIB_sinusoidGenerator_SetArgs kerSetArgs;
      kerSetArgs.startFrequency  = startFrequency;
      kerSetArgs.targetFrequency = targetFrequency;
      kerSetArgs.startPhase      = startPhase;
      kerSetArgs.smoothingTime   = smoothingTime;
      kerSetArgs.samplingRate    = samplingRate;
      kerSetArgs.data_type       = dataType;

      _handle = make_unique_handle<char>(AUDIOLIB_sinusoidGenerator_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("Sinusoidgenerator constructor", "failed to allocate sinusoidGenerator handle");
      }

      if (errorCtx.isSuccess()) {

         AUDIOLIB_bufParams1D_t bufParamsOut;

         bufParamsOut.data_type = AUDIOLIB_element_dType<T>::value;
         bufParamsOut.dim_x     = static_cast<uint32_t>(width);

         /******************************************************************************/
         /* Validate parameters before init                                            */
         /******************************************************************************/

         AUDIOLIB_STATUS checkRet =
             AUDIOLIB_sinusoidGenerator_init_checkParams(_handle.get(), &bufParamsOut, &kerInitArgs);
         errorCtx.remapXLIBError(checkRet, "AUDIOLIB_sinusoidGenerator_init_checkParams");

         /******************************************************************************/
         /* Call init function from AUDIOLIB to for sinusoidGenerator to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.isSuccess()) {
            AUDIOLIB_STATUS initRet = AUDIOLIB_sinusoidGenerator_init(_handle.get(), &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(initRet, "AUDIOLIB_sinusoidGenerator_init");
            if (errorCtx.isSuccess()) {
               AUDIOLIB_STATUS setRet = AUDIOLIB_sinusoidGenerator_set(_handle.get(), &kerSetArgs);
               errorCtx.remapXLIBError(setRet, "AUDIOLIB_sinusoidGenerator_set");
            }
         }
      }
   }
}

template <typename T>
Sinusoidgenerator<T>::Sinusoidgenerator(T          *pY,
                                        size_t      width,
                                        float       startPhase,
                                        float       smoothingTime,
                                        int32_t     samplingRate,
                                        uint32_t    executionMode,
                                        float       startFrequency,
                                        float       targetFrequency,
                                        uint32_t    dataType,
                                        const char *nodeName,
                                        uint32_t    nodeId,
                                        ErrorCtxt  &errorCtx)
    : Sinusoidgenerator(pY,
                        width,
                        startPhase,
                        smoothingTime,
                        samplingRate,
                        executionMode,
                        startFrequency,
                        targetFrequency,
                        dataType,
                        errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Sinusoidgenerator<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Sinusoidgenerator<T>::getNumOutputs() const noexcept { return 1; }

//! [TISP::Sinusoidgenerator setAddr]
template <typename T> void Sinusoidgenerator<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void Sinusoidgenerator<T>::exec() noexcept
{
   AUDIOLIB_sinusoidGenerator_exec(_handle.get(), static_cast<void *>(_pY));
}

template class Sinusoidgenerator<float>;

} // namespace AUDIOLIB
} // namespace TISP
