// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_FFTLIB_common.hpp"
#include "TISP_twiddle_lut.hpp"
#include <TISP_ifftr1dBatched.hpp>
#include <cstring>

#include <cmath>

namespace TISP {
namespace FFTLIB {

template <typename T>
IFFTR1dBatched<T>::IFFTR1dBatched(T         *pX,
                                  T         *pXFFT,
                                  T         *pY,
                                  uint32_t   numPoints,
                                  uint32_t   numChannels,
                                  bool       inMode,
                                  ErrorCtxt &errorCtx)
    : _pX(pX), _pXFFT(pXFFT), _pY(pY), _numPoints(numPoints), _numChannels(numChannels), _inMode(inMode)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numPoints=%u,numChannels=%u", numPoints,
                   numChannels);

   _pBlock = make_unique_handle<uint8_t>(TISP_IFFT1DBATCHED_I32FC_C32FC_O32F_PBLOCK_SIZE);

   if (!_pBlock) {
      errorCtx.setMemoryError("IFFTR1dBatched constructor", "failed to allocate ifftr1dBatched handle");
   }

   if (errorCtx.isSuccess()) {

      _bufParamsX.dim_x = static_cast<uint32_t>(numPoints * numChannels);

      /* Complex bins the kernel consumes per channel: N for full spectrum, N/2 + 1 for half. */
      const size_t numPointsIn = inMode ? numPoints : ((numPoints >> 1U) + 1U);

      // Initialize bufParams with node input and output buffer parameters
      // pX is the complex spectrum, one row of numPointsIn interleaved re/im pairs per channel;
      // pY is real, one row of numPoints samples per channel.
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = numPointsIn * 2U;
      _nodeInfo.bufParams.heightIn[0]       = numChannels;
      _nodeInfo.bufParams.inPitchBytes[0]   = numPointsIn * 2U * sizeof(T);

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = numPoints;
      _nodeInfo.bufParams.heightOut[0]       = numChannels;
      _nodeInfo.bufParams.outPitchBytes[0]   = numPoints * sizeof(T);

      _pW = TwiddleLUT<T>::getOrCreateRealTwiddles(_numPoints, errorCtx);
      if (_pW == nullptr) {
         errorCtx.setNullPointerError("IFFTR1dBatched constructor", "twiddleLUT pointer pW is null");
      }

      if (errorCtx.isSuccess()) {
         _pSf = TwiddleLUT<T>::getOrCreateIfftSplitFactors(_numPoints, errorCtx);
         if (_pSf == nullptr) {
            errorCtx.setNullPointerError("IFFTR1dBatched constructor", "twiddleLUT pointer pSf is null");
         }

         if (errorCtx.isSuccess()) {
            if (std::is_same<float, T>::value) {

               _bufParamsX.data_type = FFTLIB_FLOAT32;

               FFTLIB_STATUS initRet = FFTLIB_ifft1dBatched_i32fc_c32fc_o32f_init(
                   static_cast<FFTLIB_F32 *>(_pX), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pW.get()), &_bufParamsX,
                   static_cast<FFTLIB_F32 *>(_pXFFT), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pSf.get()), &_bufParamsX,
                   static_cast<FFTLIB_F32 *>(pY), &_bufParamsX, numPoints, numChannels, _inMode,
                   static_cast<void *>(_pBlock.get()));
               errorCtx.remapXLIBError(initRet, "FFTLIB_ifft1dBatched_i32fc_c32fc_o32f_init");
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
IFFTR1dBatched<T>::IFFTR1dBatched(T          *pX,
                                  T          *pXFFT,
                                  T          *pY,
                                  uint32_t    numPoints,
                                  uint32_t    numChannels,
                                  bool        inMode,
                                  const char *nodeName,
                                  uint32_t    nodeId,
                                  ErrorCtxt  &errorCtx)
    : IFFTR1dBatched(pX, pXFFT, pY, numPoints, numChannels, inMode, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void IFFTR1dBatched<T>::exec() noexcept
{

   if (std::is_same<float, T>::value) {

      FFTLIB_ifft1dBatched_i32fc_c32fc_o32f_kernel(
          static_cast<FFTLIB_F32 *>(_pX), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pW.get()), &_bufParamsX,
          static_cast<FFTLIB_F32 *>(_pXFFT), &_bufParamsX, static_cast<FFTLIB_F32 *>(_pSf.get()), &_bufParamsX,
          static_cast<FFTLIB_F32 *>(_pY), &_bufParamsX, _numPoints, _numChannels, _inMode,
          static_cast<void *>(_pBlock.get()));
   }
   else {
      // TBD: error out since not implemented!
   }

   return;
}

template <typename T> void *IFFTR1dBatched<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *IFFTR1dBatched<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t IFFTR1dBatched<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t IFFTR1dBatched<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void IFFTR1dBatched<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

/***************************/
/* Explicit instantiation  */
/***************************/

template class IFFTR1dBatched<float>;

} // namespace FFTLIB
} // namespace TISP
