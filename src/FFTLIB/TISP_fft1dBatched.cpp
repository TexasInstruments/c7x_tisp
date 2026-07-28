// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_FFTLIB_common.hpp"
#include "TISP_twiddle_lut.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_fft1dBatched.hpp>
#include <cstring>

#include <cmath>

namespace TISP {
namespace FFTLIB {

template <typename T>
FFT1dBatched<T>::FFT1dBatched(T *pX, T *pY, uint32_t numPoints, uint32_t numChannels, ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY), _numPoints(numPoints), _numChannels(numChannels)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numPoints=%u,numChannels=%u", numPoints,
                   numChannels);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("FFT1dBatched constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("FFT1dBatched constructor", "output pointer pY is null");
   }
   if (numPoints == 0) {
      errorCtx.setInvalidParameterError("FFT1dBatched constructor", "numPoints must be greater than zero");
   }
   if (numChannels == 0) {
      errorCtx.setInvalidParameterError("FFT1dBatched constructor", "numChannels must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

      _pBlock = make_unique_handle<uint8_t>(TISP_FFT1DBATCHED_I32FC_C32FC_O32FC_PBLOCK_SIZE);

      if (!_pBlock) {
         errorCtx.setMemoryError("FFT1dBatched constructor", "failed to allocate fft1dBatched handle");
      }

      if (errorCtx.isSuccess()) {

         _bufParamsX.dim_x = static_cast<uint32_t>(_numPoints * _numChannels * 2);
         _bufParamsW.dim_x = static_cast<uint32_t>(_numPoints * 2);

         // Initialize bufParams with node input and output buffer parameters.
         _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
         _nodeInfo.bufParams.isInputInterleave = 0;
         _nodeInfo.bufParams.widthIn[0]        = _numPoints * 2U;
         _nodeInfo.bufParams.heightIn[0]       = _numChannels;
         _nodeInfo.bufParams.inPitchBytes[0]   = _numPoints * 2U * sizeof(T);

         _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
         _nodeInfo.bufParams.isOutputInterleave = 0;
         _nodeInfo.bufParams.widthOut[0]        = _numPoints * 2U;
         _nodeInfo.bufParams.heightOut[0]       = _numChannels;
         _nodeInfo.bufParams.outPitchBytes[0]   = _numPoints * 2U * sizeof(T);

         _pW = TwiddleLUT<T>::getOrCreateComplexTwiddles(_numPoints, errorCtx);
         if (_pW == nullptr) {
            errorCtx.setNullPointerError("FFT1dBatched constructor", "twiddleLUT pointer pW is null");
         }

         if (errorCtx.isSuccess()) {

            if (std::is_same<float, T>::value) {

               _bufParamsX.data_type = FFTLIB_FLOAT32;
               _bufParamsW.data_type = FFTLIB_FLOAT32;

               FFTLIB_STATUS initRet = FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_init(
                   static_cast<FFTLIB_F32 *>(_pX), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pW.get()), &_bufParamsW,
                   static_cast<FFTLIB_F32 *>(_pY), &_bufParamsX, _numPoints, _numChannels,
                   static_cast<void *>(_pBlock.get()));
               errorCtx.remapXLIBError(initRet, "FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_init");
            }
            else {
               // TBD: error out since not implemented!
            }
         }
      }
   }
   return;
}

template <typename T>
FFT1dBatched<T>::FFT1dBatched(T          *pX,
                              T          *pY,
                              uint32_t    numPoints,
                              uint32_t    numChannels,
                              const char *nodeName,
                              uint32_t    nodeId,
                              ErrorCtxt  &errorCtx)
    : FFT1dBatched(pX, pY, numPoints, numChannels, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *FFT1dBatched<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *FFT1dBatched<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t FFT1dBatched<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t FFT1dBatched<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void FFT1dBatched<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void FFT1dBatched<T>::exec() noexcept
{

   if (std::is_same<float, T>::value) {

      FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_kernel(
          static_cast<FFTLIB_F32 *>(_pX), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pW.get()), &_bufParamsW,
          static_cast<FFTLIB_F32 *>(_pY), &_bufParamsX, _numPoints, _numChannels, static_cast<void *>(_pBlock.get()));
   }
   else {
      // TBD: error out since not implemented!
   }

   return;
}

/***************************/
/* Explicit instantiation  */
/***************************/

template class FFT1dBatched<float>;

} // namespace FFTLIB
} // namespace TISP
