// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_FFTLIB_common.hpp"
#include "TISP_twiddle_lut.hpp"
#include <TISP_fft1dBatched.hpp>
#include <cstdio>

namespace TISP {
namespace FFTLIB {

template <typename T>
FFT1dBatched<T>::FFT1dBatched(const TISP_BufDescList *pInputs,
                              const TISP_BufDescList *pOutputs,
                              const char             *nodeName,
                              uint32_t                nodeId,
                              ErrorCtxt              &errorCtx)
    : _in{}, _out{}, _bufParamsX{}, _bufParamsW{}, _numPoints(0U), _numChannels(0U)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   // Buffer checks: one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "FFT1dBatched constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "FFT1dBatched constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      if ((_in.isInterleave != 0U) || (_out.isInterleave != 0U)) {
         errorCtx.setInvalidParameterError("FFT1dBatched constructor", "input and output must hold one channel per row");
      }
      else if ((_in.width % 2U) != 0U) {
         errorCtx.setInvalidParameterError("FFT1dBatched constructor",
                                           "input width must be even (interleaved real/imaginary pairs)");
      }
      else if ((_out.width != _in.width) || (_out.height != _in.height)) {
         errorCtx.setInvalidParameterError("FFT1dBatched constructor", "output width and height must match the input");
      }
      else if (!TISP_isPackedRows<T>(_in) || !TISP_isPackedRows<T>(_out)) {
         errorCtx.setInvalidParameterError("FFT1dBatched constructor", "input and output rows must be packed");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _numPoints   = static_cast<uint32_t>(_in.width / 2U);
      _numChannels = static_cast<uint32_t>(_in.height);

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numPoints=%u,numChannels=%u", _numPoints,
                      _numChannels);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      _bufParamsX.data_type = FFTLIB_FLOAT32;
      _bufParamsX.dim_x     = _numPoints * _numChannels * 2U;
      _bufParamsW.data_type = FFTLIB_FLOAT32;
      _bufParamsW.dim_x     = _numPoints * 2U;

      _block = make_unique_handle<uint8_t>(TISP_FFT1DBATCHED_I32FC_C32FC_O32FC_PBLOCK_SIZE);

      if (!_block) {
         errorCtx.setMemoryError("FFT1dBatched constructor", "failed to allocate fft1dBatched handle");
      }
      else {
         _twiddles = TwiddleLUT<T>::getOrCreateComplexTwiddles(_numPoints, errorCtx);

         if (errorCtx.isSuccess() && (_twiddles == nullptr)) {
            errorCtx.setNullPointerError("FFT1dBatched constructor", "twiddle factor pointer is null");
         }
         else if (errorCtx.isSuccess()) {
            FFTLIB_STATUS status = FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_init(
                static_cast<FFTLIB_F32 *>(_in.pBuf), &_bufParamsX, static_cast<FFTLIB_F32 *>(_twiddles.get()),
                &_bufParamsW, static_cast<FFTLIB_F32 *>(_out.pBuf), &_bufParamsX, _numPoints, _numChannels,
                static_cast<void *>(_block.get()));
            errorCtx.remapXLIBError(status, "FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_init");
         }
         else {
            /* Nothing to do here */
         }
      }
   }
}

template <typename T> void *FFT1dBatched<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *FFT1dBatched<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t FFT1dBatched<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t FFT1dBatched<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void FFT1dBatched<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addr.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void FFT1dBatched<T>::exec() noexcept
{
   FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_kernel(static_cast<FFTLIB_F32 *>(_in.pBuf), &_bufParamsX,
                                                static_cast<FFTLIB_F32 *>(_twiddles.get()), &_bufParamsW,
                                                static_cast<FFTLIB_F32 *>(_out.pBuf), &_bufParamsX, _numPoints,
                                                _numChannels, static_cast<void *>(_block.get()));
}

/***************************/
/* Explicit instantiation  */
/***************************/

template class FFT1dBatched<float>;

} // namespace FFTLIB
} // namespace TISP
