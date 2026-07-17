// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_FFTLIB_common.hpp"
#include "TISP_twiddle_lut.hpp"
#include <TISP_rfft1dBatched.hpp>
#include <cstring>

#include <cmath>
#include <cstdint>

namespace TISP {
namespace FFTLIB {

template <typename T>
RFFT1dBatched<T>::RFFT1dBatched(T         *pX,
                                T         *pXFFT,
                                T         *pY,
                                uint32_t   numPoints,
                                uint32_t   numChannels,
                                bool       outMode,
                                ErrorCtxt &errorCtx)
    : _pX(pX), _pXFFT(pXFFT), _pY(pY), _numPoints(numPoints), _numChannels(numChannels), _outMode(outMode)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numPoints=%u,numChannels=%u,outMode=%s",
                   numPoints, numChannels, outMode ? "true" : "false");

   _pBlock = make_unique_handle<uint8_t>(TISP_FFT1DBATCHED_I32F_C32FC_O32FC_PBLOCK_SIZE);

   if (!_pBlock) {
      errorCtx.setMemoryError("RFFT1dBatched constructor", "failed to allocate rfft1dBatched handle");
   }

   if (errorCtx.isSuccess()) {

      _bufParamsX.dim_x = static_cast<uint32_t>(numPoints * numChannels);

      // Initialize bufParams with node input and output buffer parameters
      // pX is a real buffer (numPoints * numChannels elements); pY is the complex spectrum, sized for the
      // full-spectrum case (numPoints * numChannels * 2 elements) regardless of outMode.
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = _bufParamsX.dim_x;
      _nodeInfo.bufParams.heightIn[0]       = 1;
      _nodeInfo.bufParams.inPitchBytes[0]   = _bufParamsX.dim_x * sizeof(T);

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = _bufParamsX.dim_x * 2U;
      _nodeInfo.bufParams.heightOut[0]       = 1;
      _nodeInfo.bufParams.outPitchBytes[0]   = _bufParamsX.dim_x * 2U * sizeof(T);

      _pW = TwiddleLUT<T>::getOrCreateRealTwiddles(_numPoints, errorCtx);
      if (_pW == nullptr) {
         errorCtx.setNullPointerError("RFFT1dBatched constructor", "twiddleLUT pointer pW is null");
      }

      if (errorCtx.isSuccess()) {

         _pSf = TwiddleLUT<T>::getOrCreateRfftSplitFactors(_numPoints, errorCtx);
         if (_pSf == nullptr) {
            errorCtx.setNullPointerError("RFFT1dBatched constructor", "twiddleLUT pointer pSf is null");
         }

         if (errorCtx.isSuccess()) {

            if (std::is_same<float, T>::value) {

               _bufParamsX.data_type = FFTLIB_FLOAT32;

               FFTLIB_STATUS initRet = FFTLIB_fft1dBatched_i32f_c32fc_o32fc_init(
                   static_cast<FFTLIB_F32 *>(pX), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pW.get()), &_bufParamsX,
                   static_cast<FFTLIB_F32 *>(pXFFT), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pSf.get()), &_bufParamsX,
                   static_cast<FFTLIB_F32 *>(pY), &_bufParamsX, _numPoints, _numChannels, _outMode,
                   static_cast<void *>(_pBlock.get()));
               errorCtx.remapXLIBError(initRet, "FFTLIB_fft1dBatched_i32f_c32fc_o32fc_init");
            }
            else {
               // TBD: error out since not implemented!
            }
         }
      }
   }
   return;
}

template <typename T> void *RFFT1dBatched<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *RFFT1dBatched<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t RFFT1dBatched<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t RFFT1dBatched<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void RFFT1dBatched<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T>
RFFT1dBatched<T>::RFFT1dBatched(T          *pX,
                                T          *pXFFT,
                                T          *pY,
                                uint32_t    numPoints,
                                uint32_t    numChannels,
                                bool        outMode,
                                const char *nodeName,
                                uint32_t    nodeId,
                                ErrorCtxt  &errorCtx)
    : RFFT1dBatched(pX, pXFFT, pY, numPoints, numChannels, outMode, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void RFFT1dBatched<T>::exec() noexcept
{

   if (std::is_same<float, T>::value) {

      FFTLIB_fft1dBatched_i32f_c32fc_o32fc_kernel(
          static_cast<FFTLIB_F32 *>(_pX), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pW.get()), &_bufParamsX,
          static_cast<FFTLIB_F32 *>(_pXFFT), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pSf.get()), &_bufParamsX,
          static_cast<FFTLIB_F32 *>(_pY), &_bufParamsX, _numPoints, _numChannels, _outMode,
          static_cast<void *>(_pBlock.get()));
   }
   else {
      // TBD: error out since not implemented!
   }

   return;
}

/***************************/
/* Explicit instantiation  */
/***************************/

template class RFFT1dBatched<float>;

} // namespace FFTLIB
} // namespace TISP
