// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <DSPLIB/TISP_DSPLIB_types.hpp>
#include <TISP_cascadeBiquad.hpp>
#include <cstdint>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [DSPLIB_matTrans constructor]

template <typename T>
CascadeBiquad<T>::CascadeBiquad(T         *pIn,
                                T         *pFilterCoeff,
                                T         *pFilterVar,
                                T         *pOut,
                                size_t     size,
                                size_t     numChannels,
                                size_t     numStages,
                                uint32_t   biquadForm,
                                size_t     dataPitchInBytes,
                                size_t     dataPitchOutBytes,
                                size_t     filterVarPitchBytes,
                                bool       enableNChCoeffs,
                                ErrorCtxt &errorCtx)
    : _pIn(pIn), _pFilterCoeff(pFilterCoeff), _pFilterVar(pFilterVar), _pOut(pOut), _coeffSizeBytes(0),
      _historyBufSize(0)

{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "size=%zu,numChannels=%zu,biquadForm=%u", size,
                   numChannels, biquadForm);

   // Input validation checks
   if (pIn == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad constructor", "input pointer pIn is null");
   }
   else if (pFilterCoeff == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad constructor", "filter coefficients pointer pFilterCoeff is null");
   }
   else if (pFilterVar == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad constructor", "state pointer pFilterVar is null");
   }
   else if (pOut == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad constructor", "output pointer pOut is null");
   }
   else if (size == 0 || numChannels == 0) {
      errorCtx.setInvalidParameterError("CascadeBiquad constructor", "size and numChannels must be greater than zero");
   }
   else {
      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_matrTrans from DSPLIB */
      /***********************************************************************/

      DSPLIB_cascadeBiquad_InitArgs kerInitArgs;
      DSPLIB_bufParams2D_t          bufParamsIn;
      DSPLIB_bufParams2D_t          bufParamsOut;
      DSPLIB_bufParams2D_t          bufParamsFilterVar;
      DSPLIB_bufParams2D_t          bufParamsFilterCoeff;

      kerInitArgs.funcStyle       = DSPLIB_FUNCTION_OPTIMIZED;
      kerInitArgs.dataSize        = size;
      kerInitArgs.numChannels     = numChannels;
      kerInitArgs.numStages       = numStages;
      kerInitArgs.enableNChCoeffs = enableNChCoeffs;
      kerInitArgs.biquadForm      = biquadForm;

      bufParamsIn.data_type = DSPLIB_element_dType<T>::value;
      bufParamsIn.dim_x     = static_cast<uint32_t>(numChannels);
      bufParamsIn.dim_y     = static_cast<uint32_t>(size);
      bufParamsIn.stride_y  = static_cast<uint32_t>(dataPitchInBytes);

      bufParamsOut.data_type = DSPLIB_element_dType<T>::value;
      bufParamsOut.dim_x     = static_cast<uint32_t>(numChannels);
      bufParamsOut.dim_y     = static_cast<uint32_t>(size);
      bufParamsOut.stride_y  = static_cast<uint32_t>(dataPitchOutBytes);

      bufParamsFilterVar.data_type = DSPLIB_element_dType<T>::value;
      bufParamsFilterVar.dim_x     = static_cast<uint32_t>(numChannels);
      bufParamsFilterVar.dim_y     = (biquadForm == static_cast<uint32_t>(DSPLIB_BIQUAD_DF1))
                                         ? numStages * static_cast<size_t>(DSPLIB_NUM_STATE_VARS_DF1)
                                         : numStages * static_cast<size_t>(DSPLIB_NUM_STATE_VARS_DF2);
      bufParamsFilterVar.stride_y  = static_cast<uint32_t>(filterVarPitchBytes);

      DSPLIB_STATUS status = DSPLIB_cascadeBiquad_get_coefficientsBufferDesc(
          DSPLIB_element_dType<T>::value, numStages, numChannels, biquadForm, enableNChCoeffs, &bufParamsFilterCoeff);
      errorCtx.remapXLIBError(status, "DSPLIB_cascadeBiquad_get_coefficientsBufferDesc");

      if (errorCtx.isSuccess()) {
         _coeffSizeBytes = bufParamsFilterCoeff.dim_y * bufParamsFilterCoeff.stride_y;
         _historyBufSize = bufParamsFilterVar.stride_y * bufParamsFilterVar.dim_y;

         // Initialize bufParams with node input and output buffer parameters.
         _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
         _nodeInfo.bufParams.isInputInterleave = 1;
         _nodeInfo.bufParams.widthIn[0]        = numChannels;
         _nodeInfo.bufParams.heightIn[0]       = size;
         _nodeInfo.bufParams.inPitchBytes[0]   = dataPitchInBytes;

         _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
         _nodeInfo.bufParams.isOutputInterleave = 1;
         _nodeInfo.bufParams.widthOut[0]        = numChannels;
         _nodeInfo.bufParams.heightOut[0]       = size;
         _nodeInfo.bufParams.outPitchBytes[0]   = dataPitchOutBytes;

         /******************************************************************************/
         /* Call init function from DSPLIB cascade biquad  to set SE/SA params         */
         /******************************************************************************/

         _handle = make_unique_handle<char>(DSPLIB_cascadeBiquad_getHandleSize(&kerInitArgs));

         if (!_handle) {
            errorCtx.setMemoryError("CascadeBiquad constructor", "failed to allocate cascadeBiquad handle");
         }

         if (errorCtx.isSuccess()) {
            status = DSPLIB_cascadeBiquad_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsFilterCoeff,
                                                           &bufParamsFilterVar, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_cascadeBiquad_init_checkParams");

            if (errorCtx.isSuccess()) {
               status = DSPLIB_cascadeBiquad_init(_handle.get(), &bufParamsIn, &bufParamsFilterCoeff,
                                                  &bufParamsFilterVar, &bufParamsOut, &kerInitArgs);
               errorCtx.remapXLIBError(status, "DSPLIB_cascadeBiquad_init");
               if (errorCtx.isSuccess()) {
                  cleanInternalBuffers();
               }
            }
         }
      }
   }
}

//! [DSPLIB_CascadeBiquad constructor]

template <typename T> void *CascadeBiquad<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pIn) : nullptr;
}

template <typename T> void *CascadeBiquad<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pOut) : nullptr;
}

template <typename T> size_t CascadeBiquad<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t CascadeBiquad<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void CascadeBiquad<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pIn = static_cast<T *>(addr.src[0]);
   }
   if (mode == kAddrModeOut) {
      _pOut = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pIn          = static_cast<T *>(addr.src[0]);
      _pFilterCoeff = static_cast<T *>(addr.src[1]);
      _pFilterVar   = static_cast<T *>(addr.src[2]);
      _pOut         = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T>
CascadeBiquad<T>::CascadeBiquad(T          *pIn,
                                T          *pFilterCoeff,
                                T          *pFilterVar,
                                T          *pOut,
                                size_t      size,
                                size_t      numChannels,
                                size_t      numStages,
                                uint32_t    biquadForm,
                                size_t      dataPitchInBytes,
                                size_t      dataPitchOutBytes,
                                size_t      filterVarPitchBytes,
                                bool        enableNChCoeffs,
                                const char *nodeName,
                                uint32_t    nodeId,
                                ErrorCtxt  &errorCtx)
    : CascadeBiquad(pIn,
                    pFilterCoeff,
                    pFilterVar,
                    pOut,
                    size,
                    numChannels,
                    numStages,
                    biquadForm,
                    dataPitchInBytes,
                    dataPitchOutBytes,
                    filterVarPitchBytes,
                    enableNChCoeffs,
                    errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void CascadeBiquad<T>::exec() noexcept
{

   DSPLIB_cascadeBiquad_exec(_handle.get(), _pIn, _pFilterCoeff, _pFilterVar, _pOut);
}

template <typename T>
bool CascadeBiquad<T>::setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad::setParams", "value pointer is null");
   }
   else {
      switch (static_cast<CascadeBiquadParamId>(paramId)) {
      case CascadeBiquadParamId::kFilterCoeffs: {
         if (size < _coeffSizeBytes) {
            errorCtx.setInvalidParameterError("CascadeBiquad::setParams",
                                              "Value size smaller than expected for kFilterCoeffs");
         }
         else {
            cleanInternalBuffers();
            memcpy(_pFilterCoeff, value, _coeffSizeBytes);
         }
         break;
      }
      default:
         errorCtx.setInvalidParameterError("CascadeBiquad::setParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T>
bool CascadeBiquad<T>::getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
{
   if (value == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad::getParams", "value pointer is null");
   }
   else if (size == nullptr) {
      errorCtx.setNullPointerError("CascadeBiquad::getParams", "size pointer is null");
   }
   else {
      switch (static_cast<CascadeBiquadParamId>(paramId)) {
      case CascadeBiquadParamId::kFilterCoeffs: {
         if (*size < _coeffSizeBytes) {
            errorCtx.setInvalidParameterError("CascadeBiquad::getParams", "Provided buffer size is too small");
         }
         else {
            memcpy(value, _pFilterCoeff, _coeffSizeBytes);
         }
         *size = _coeffSizeBytes;
         break;
      }
      default:
         errorCtx.setInvalidParameterError("CascadeBiquad::getParams", "Invalid paramId");
         break;
      }
   }

   return errorCtx.isSuccess();
}

template <typename T> void CascadeBiquad<T>::cleanInternalBuffers() noexcept
{
   if (_pFilterVar != nullptr && _historyBufSize > 0) {
      std::memset(_pFilterVar, 0, _historyBufSize);
   }
}

template class CascadeBiquad<float>;

} // namespace DSPLIB
} // namespace TISP
