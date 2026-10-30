// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_FFTLIB_common.hpp"
#include "TISP_twiddle_lut.hpp"
#include <TISP_rfft1dBatched.hpp>
#include <cstdint>
#include <cstdio>

namespace TISP {
namespace FFTLIB {

template <typename T>
RFFT1dBatched<T>::RFFT1dBatched(const TISP_BufDescList         *pInputs,
                                const TISP_BufDescList         *pOutputs,
                                void                           *pScratch,
                                size_t                          scratchBytes,
                                const RFFT1dBatchedConfig *pConfig,
                                const char                     *nodeName,
                                uint32_t                        nodeId,
                                ErrorCtxt                      &errorCtx)
    : _in{}, _out{}, _scratch(static_cast<T *>(pScratch)), _bufParamsX{}, _numPoints(0U), _numChannels(0U),
      _fullSpectrum(false)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("RFFT1dBatched constructor", "build params pointer pConfig is null");
   }
   else if (pScratch == nullptr) {
      errorCtx.setNullPointerError("RFFT1dBatched constructor", "scratch pointer pScratch is null");
   }
   else {
      /* Nothing to do here */
   }

   // Buffer checks (skipped once an error is set): one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "RFFT1dBatched constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "RFFT1dBatched constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      // Complex bins per channel: N for the full spectrum, N/2 + 1 for the half spectrum
      const size_t numBins = pConfig->fullSpectrum ? _in.width : ((_in.width / 2U) + 1U);

      if ((_in.isInterleave != 0U) || (_out.isInterleave != 0U)) {
         errorCtx.setInvalidParameterError("RFFT1dBatched constructor",
                                           "input and output must hold one channel per row");
      }
      else if ((_out.width != (2U * numBins)) || (_out.height != _in.height)) {
         errorCtx.setInvalidParameterError("RFFT1dBatched constructor",
                                           "output must hold 2 x (number of bins) values per input row");
      }
      else if (!TISP_isPackedRows<T>(_in) || !TISP_isPackedRows<T>(_out)) {
         errorCtx.setInvalidParameterError("RFFT1dBatched constructor", "input and output rows must be packed");
      }
      else if (scratchBytes < (_in.width * _in.height * sizeof(T))) {
         errorCtx.setInvalidParameterError("RFFT1dBatched constructor",
                                           "scratchBytes is smaller than input width x height");
      }
      else {
         /* Nothing to do here */
      }
   }

   if (errorCtx.isSuccess()) {
      _numPoints    = static_cast<uint32_t>(_in.width);
      _numChannels  = static_cast<uint32_t>(_in.height);
      _fullSpectrum = pConfig->fullSpectrum;

      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "numPoints=%u,numChannels=%u,outMode=%s",
                      _numPoints, _numChannels, _fullSpectrum ? "true" : "false");
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      _bufParamsX.data_type = FFTLIB_FLOAT32;
      _bufParamsX.dim_x     = _numPoints * _numChannels;

      _block = make_unique_handle<uint8_t>(TISP_FFT1DBATCHED_I32F_C32FC_O32FC_PBLOCK_SIZE);

      if (!_block) {
         errorCtx.setMemoryError("RFFT1dBatched constructor", "failed to allocate rfft1dBatched handle");
      }
      else {
         _twiddles = TwiddleLUT<T>::getOrCreateRealTwiddles(_numPoints, errorCtx);
         if (errorCtx.isSuccess()) {
            _splitFactors = TwiddleLUT<T>::getOrCreateRfftSplitFactors(_numPoints, errorCtx);
         }

         if (errorCtx.isSuccess() && ((_twiddles == nullptr) || (_splitFactors == nullptr))) {
            errorCtx.setNullPointerError("RFFT1dBatched constructor", "twiddle or split factor pointer is null");
         }
         else if (errorCtx.isSuccess()) {
            FFTLIB_STATUS status = FFTLIB_fft1dBatched_i32f_c32fc_o32fc_init(
                static_cast<FFTLIB_F32 *>(_in.pBuf), &_bufParamsX, static_cast<FFTLIB_F32 *>(_twiddles.get()),
                &_bufParamsX, static_cast<FFTLIB_F32 *>(_scratch), &_bufParamsX,
                static_cast<FFTLIB_F32 *>(_splitFactors.get()), &_bufParamsX, static_cast<FFTLIB_F32 *>(_out.pBuf),
                &_bufParamsX, _numPoints, _numChannels, _fullSpectrum, static_cast<void *>(_block.get()));
            errorCtx.remapXLIBError(status, "FFTLIB_fft1dBatched_i32f_c32fc_o32fc_init");
         }
         else {
            /* Nothing to do here */
         }
      }
   }
}

template <typename T> void *RFFT1dBatched<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *RFFT1dBatched<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t RFFT1dBatched<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t RFFT1dBatched<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void RFFT1dBatched<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _in.pBuf = addr.src[0];
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void RFFT1dBatched<T>::exec() noexcept
{
   FFTLIB_fft1dBatched_i32f_c32fc_o32fc_kernel(
       static_cast<FFTLIB_F32 *>(_in.pBuf), &_bufParamsX, static_cast<FFTLIB_F32 *>(_twiddles.get()), &_bufParamsX,
       static_cast<FFTLIB_F32 *>(_scratch), &_bufParamsX, static_cast<FFTLIB_F32 *>(_splitFactors.get()), &_bufParamsX,
       static_cast<FFTLIB_F32 *>(_out.pBuf), &_bufParamsX, _numPoints, _numChannels, _fullSpectrum,
       static_cast<void *>(_block.get()));
}

/***************************/
/* Explicit instantiation  */
/***************************/

template class RFFT1dBatched<float>;

} // namespace FFTLIB
} // namespace TISP
