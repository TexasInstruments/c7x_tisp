// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_istft.hpp>
#include <c7x.h>
#include <cstdint>
#include <cstring>
#include <memory>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
Istft<T>::Istft(T         *pX,
                T         *pY,
                T         *pWindow,
                T         *pOlaBuf,
                T         *pScratch,
                uint32_t   fftSize,
                uint32_t   hopSize,
                uint32_t   numChannels,
                uint32_t   numWindows,
                bool       onesided,
                ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY), _pWindow(pWindow), _pOlaBuf(pOlaBuf), _fftSize(fftSize), _hopSize(hopSize),
      _numChannels(numChannels), _numWindows(numWindows), _onesided(onesided)
{
   /* input validation */
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Istft constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Istft constructor", "output pointer pY is null");
   }
   if (pWindow == nullptr) {
      errorCtx.setNullPointerError("Istft constructor", "window pointer pWindow is null");
   }
   if (pOlaBuf == nullptr) {
      errorCtx.setNullPointerError("Istft constructor", "overlap-add pointer pOlaBuf is null");
   }
   if (pScratch == nullptr) {
      errorCtx.setNullPointerError("Istft constructor", "scratch pointer pScratch is null");
   }
   if (fftSize == 0u) {
      errorCtx.setInvalidParameterError("Istft constructor", "fftSize must be greater than zero");
   }
   if (hopSize == 0u) {
      errorCtx.setInvalidParameterError("Istft constructor", "hopSize must be greater than zero");
   }
   else if (hopSize > fftSize) {
      errorCtx.setInvalidParameterError("Istft constructor", "hopSize must not exceed fftSize");
   }
   if (numWindows == 0u) {
      errorCtx.setInvalidParameterError("Istft constructor", "numWindows must be >= 1");
   }
   if (numChannels == 0u) {
      errorCtx.setInvalidParameterError("Istft constructor", "numChannels must be greater than zero");
   }

   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "fftSize=%u,hopSize=%u,numCh=%u,numWindows=%u,onesided=%s", fftSize, hopSize, numChannels,
                   numWindows, onesided ? "true" : "false");

   if (errorCtx.isSuccess()) {
      /* derived constants via static calc functions (single source of truth) — computed only
       * after validation so calcOlaBufSize cannot underflow on hopSize > fftSize */
      _inBins           = onesided ? (fftSize + 2u) : (2u * fftSize);
      _overlapBufSize   = calcOlaBufSize(fftSize, hopSize);
      _numOutputSamples = calcNumOutputSamples(numWindows, hopSize);

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = _numWindows * _inBins;
      _nodeInfo.bufParams.heightIn[0]       = _numChannels;
      _nodeInfo.bufParams.inPitchBytes[0]   = _numWindows * _inBins * sizeof(T);

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = _numOutputSamples;
      _nodeInfo.bufParams.heightOut[0]       = _numChannels;
      _nodeInfo.bufParams.outPitchBytes[0]   = _numOutputSamples * sizeof(T);

      /* when numWindows==1 and numChannels>1, batch the inverse FFT across channels instead
       * of looping per channel; batchSize drives scratch, IFFT count, and SE/SA setup. */
      bool     isBatch   = (numWindows == 1u) && (numChannels > 1u);
      uint32_t batchSize = isBatch ? numChannels : numWindows;
      _execImpl          = isBatch ? &Istft::execBatchChannels : &Istft::execMultiWindow;

      /* zero-initialize OLA state so the first exec() sees silence in the tail */
      _historyBufSize = static_cast<size_t>(_overlapBufSize) * numChannels;
      cleanInternalBuffers();

      /* steady-state window envelope, stored reciprocated so step 3.3 multiplies:
       *   _pEnvelope[j] = 1 / (sum_i _pWindow[j + i*hopSize]^2 + eps),  j in [0, hopSize).
       * signal-independent, so compute once here rather than per exec(). */
      _normEps = static_cast<T>(1e-8);

      /* split caller's scratch buffer; round up first region to SE/SA alignment boundary.
       *   region 1 (_pIFFTOut)    : real inverse-FFT output frames (numWindows * fftSize)
       *   region 2 (_pFftScratch) : IFFTR butterfly scratch, reused as windowed frames
       *   region 3 (_pEnvelope)   : steady-state window envelope (hopSize, <= fftSize) */
      constexpr uint32_t alignElems      = static_cast<uint32_t>(TISP_SE_SA_ALIGN_BYTES / sizeof(T));
      uint32_t           firstHalfElems  = ((batchSize * fftSize) + alignElems - 1u) & ~(alignElems - 1u);
      uint32_t           secondHalfElems = batchSize * (fftSize + 1u) * 2u;
      _pIFFTOut                          = pScratch;
      _pFftScratch                       = &pScratch[firstHalfElems];
      _pEnvelope                         = &pScratch[firstHalfElems + secondHalfElems];

      for (uint32_t j = 0; j < _hopSize; j++) {
         T acc = static_cast<T>(0);

         for (uint32_t idx = j; idx < _fftSize; idx += _hopSize) {
            acc += _pWindow[idx] * _pWindow[idx];
         }

         _pEnvelope[j] = acc + _normEps;
         _pEnvelope[j] = static_cast<T>(1) / _pEnvelope[j];
      }

      /* compose the inverse real FFT node: complex spectrum -> real time-domain frames,
       * batched over batchSize frames.  pX (input) is rebound in exec(); pY (=_pIFFTOut) and
       * the butterfly scratch (=_pFftScratch) are fixed.  inMode = !onesided (0 = half spectrum). */
      const uint32_t         inBins      = onesided ? (fftSize + 2u) : (2u * fftSize);
      const TISP_BufDesc2D   ifftIn      = {pX, ElementDType<T>::value, inBins, batchSize,
                                             static_cast<size_t>(inBins) * sizeof(T), 0U};
      const TISP_BufDesc2D   ifftOut     = {_pIFFTOut, ElementDType<T>::value, fftSize, batchSize,
                                             static_cast<size_t>(fftSize) * sizeof(T), 0U};
      const TISP_BufDescList ifftInputs  = {&ifftIn, 1U};
      const TISP_BufDescList ifftOutputs = {&ifftOut, 1U};

      TISP::FFTLIB::IFFTR1dBatchedConfig ifftBuild;
      ifftBuild.fullSpectrum = !onesided;

      _ifftNode = TISP::make_unique_node<TISP::FFTLIB::IFFTR1dBatched<T>>(
          &ifftInputs, &ifftOutputs, _pFftScratch, static_cast<size_t>(secondHalfElems) * sizeof(T), &ifftBuild,
          "IFFTR1dBatched", 0U, errorCtx);
      /* make_unique_node returns null on L2 node-arena exhaustion without touching
       * errorCtx, so a null result must be reported explicitly as a memory error */
      if (_ifftNode == nullptr) {
         errorCtx.setMemoryError("Istft constructor", "failed to allocate IFFTR1dBatched node from L2 node arena");
      }

      /* SE/SA templates for synthesis windowing (shared by both exec paths, batchSize frames):
       *   SE0: 1D read of all IFFT output frames
       *   SE1: 2D repeated read of the synthesis window (DIM1=0 restarts each frame)
       *   SA0: 1D write of windowed frames to _pFftScratch */
      _seFrames.ELETYPE = c7x::se_eletype<vec>::value;
      _seFrames.VECLEN  = c7x::se_veclen<vec>::value;
      _seFrames.DIMFMT  = __SE_DIMFMT_1D;
      _seFrames.ICNT0   = batchSize * fftSize;

      _seWindow.ELETYPE = c7x::se_eletype<vec>::value;
      _seWindow.VECLEN  = c7x::se_veclen<vec>::value;
      _seWindow.DIMFMT  = __SE_DIMFMT_2D;
      _seWindow.ICNT0   = fftSize;
      _seWindow.ICNT1   = batchSize;
      _seWindow.DIM1    = 0; /* restart the window pointer each frame */

      _saOutput.VECLEN = c7x::sa_veclen<vec>::value;
      _saOutput.DIMFMT = __SA_DIMFMT_1D;
      _saOutput.ICNT0  = batchSize * fftSize;

      _numWindowVecs = (batchSize * fftSize + c7x::max_simd<T>::value - 1u) / c7x::max_simd<T>::value;

      if (!isBatch) {
         /* step 3.2 overlap-add scatter templates. */
         _seScatterFrame.ELETYPE = c7x::se_eletype<vec>::value;
         _seScatterFrame.VECLEN  = c7x::se_veclen<vec>::value;
         _seScatterFrame.DIMFMT  = __SE_DIMFMT_2D;
         _seScatterFrame.ICNT0   = fftSize;
         _seScatterFrame.ICNT1   = numWindows;
         _seScatterFrame.DIM1    = static_cast<int32_t>(fftSize);

         _seScatterAcc.ELETYPE = c7x::se_eletype<vec>::value;
         _seScatterAcc.VECLEN  = c7x::se_veclen<vec>::value;
         _seScatterAcc.DIMFMT  = __SE_DIMFMT_2D;
         _seScatterAcc.ICNT0   = fftSize;
         _seScatterAcc.ICNT1   = numWindows;
         _seScatterAcc.DIM1    = static_cast<int32_t>(hopSize);

         _saScatter.VECLEN = c7x::sa_veclen<vec>::value;
         _saScatter.DIMFMT = __SA_DIMFMT_2D;
         _saScatter.ICNT0  = fftSize;
         _saScatter.ICNT1  = numWindows;
         _saScatter.DIM1   = static_cast<int32_t>(hopSize);

         /* step 3.2 walks frames in an outer loop and vectors in an inner loop, so it needs the
          * per-frame count; the flat numWindows*vecsPerFrame product is only used by the batch path. */
         _scatterVecsPerFrame = (fftSize + c7x::max_simd<T>::value - 1u) / c7x::max_simd<T>::value;

         /* step 3.3 envelope normalization templates */
         uint32_t vecElems         = c7x::max_simd<T>::value;
         uint32_t icnt0Emit        = (hopSize < vecElems) ? hopSize : vecElems;
         uint32_t emitVecsPerFrame = (hopSize + vecElems - 1u) / vecElems;

         _seEmitWork.ELETYPE       = c7x::se_eletype<vec>::value;
         _seEmitWork.VECLEN        = c7x::se_veclen<vec>::value;
         _seEmitWork.DIMFMT        = __SE_DIMFMT_3D;
         _seEmitWork.ICNT0         = icnt0Emit;
         _seEmitWork.ICNT1         = numWindows;
         _seEmitWork.DIM1          = static_cast<int32_t>(hopSize);
         _seEmitWork.ICNT2         = emitVecsPerFrame;
         _seEmitWork.DIM2          = static_cast<int32_t>(vecElems);
         _seEmitWork.DECDIM1_WIDTH = hopSize;
         _seEmitWork.DECDIM1       = __SE_DECDIM_DIM2;

         _seEmitEnv.ELETYPE       = c7x::se_eletype<vec>::value;
         _seEmitEnv.VECLEN        = c7x::se_veclen<vec>::value;
         _seEmitEnv.DIMFMT        = __SE_DIMFMT_3D;
         _seEmitEnv.ICNT0         = icnt0Emit;
         _seEmitEnv.ICNT1         = numWindows;
         _seEmitEnv.DIM1          = 0; /* re-read the per-hop envelope each frame */
         _seEmitEnv.ICNT2         = emitVecsPerFrame;
         _seEmitEnv.DIM2          = static_cast<int32_t>(vecElems);
         _seEmitEnv.DECDIM1_WIDTH = hopSize;
         _seEmitEnv.DECDIM1       = __SE_DECDIM_DIM2;

         _saEmit.VECLEN        = c7x::sa_veclen<vec>::value;
         _saEmit.DIMFMT        = __SA_DIMFMT_3D;
         _saEmit.ICNT0         = icnt0Emit;
         _saEmit.ICNT1         = numWindows;
         _saEmit.DIM1          = static_cast<int32_t>(hopSize);
         _saEmit.ICNT2         = emitVecsPerFrame;
         _saEmit.DIM2          = static_cast<int32_t>(vecElems);
         _saEmit.DECDIM1_WIDTH = hopSize;
         _saEmit.DECDIM1       = __SA_DECDIM_DIM2;

         _numEmitVecs = numWindows * emitVecsPerFrame;
      }
      else {
         /* batch-channels path (numWindows==1). */
         uint32_t vecElems = c7x::max_simd<T>::value;

         /* 3.2 add: workbuf[i] += windowed[i] over numChannels*fftSize */
         _seScatterFrame.ELETYPE = c7x::se_eletype<vec>::value;
         _seScatterFrame.VECLEN  = c7x::se_veclen<vec>::value;
         _seScatterFrame.DIMFMT  = __SE_DIMFMT_1D;
         _seScatterFrame.ICNT0   = numChannels * fftSize;

         _seScatterAcc.ELETYPE = c7x::se_eletype<vec>::value;
         _seScatterAcc.VECLEN  = c7x::se_veclen<vec>::value;
         _seScatterAcc.DIMFMT  = __SE_DIMFMT_1D;
         _seScatterAcc.ICNT0   = numChannels * fftSize;

         _saScatter.VECLEN = c7x::sa_veclen<vec>::value;
         _saScatter.DIMFMT = __SA_DIMFMT_1D;
         _saScatter.ICNT0  = numChannels * fftSize;

         _numScatterVecs = (numChannels * fftSize + vecElems - 1u) / vecElems;

         /* 3.3 emit: out[ch*hop + s] = workbuf[ch*fftSize + s] * env[s]. */
         _seEmitWork.ELETYPE = c7x::se_eletype<vec>::value;
         _seEmitWork.VECLEN  = c7x::se_veclen<vec>::value;
         _seEmitWork.DIMFMT  = __SE_DIMFMT_2D;
         _seEmitWork.ICNT0   = hopSize;
         _seEmitWork.ICNT1   = numChannels;
         _seEmitWork.DIM1    = static_cast<int32_t>(fftSize);

         _seEmitEnv.ELETYPE = c7x::se_eletype<vec>::value;
         _seEmitEnv.VECLEN  = c7x::se_veclen<vec>::value;
         _seEmitEnv.DIMFMT  = __SE_DIMFMT_2D;
         _seEmitEnv.ICNT0   = hopSize;
         _seEmitEnv.ICNT1   = numChannels;
         _seEmitEnv.DIM1    = 0; /* re-read the per-hop envelope each channel */

         _saEmit.VECLEN = c7x::sa_veclen<vec>::value;
         _saEmit.DIMFMT = __SA_DIMFMT_2D;
         _saEmit.ICNT0  = hopSize;
         _saEmit.ICNT1  = numChannels;
         _saEmit.DIM1   = static_cast<int32_t>(hopSize);

         _numEmitVecs = numChannels * ((hopSize + vecElems - 1u) / vecElems);

         _seStateCopy.ELETYPE = c7x::se_eletype<vec>::value;
         _seStateCopy.VECLEN  = c7x::se_veclen<vec>::value;
         _seStateCopy.DIMFMT  = __SE_DIMFMT_2D;
         _seStateCopy.ICNT0   = _overlapBufSize;
         _seStateCopy.ICNT1   = numChannels;
         _seStateCopy.DIM1    = static_cast<int32_t>(_overlapBufSize);

         _saStateCopy.VECLEN = c7x::sa_veclen<vec>::value;
         _saStateCopy.DIMFMT = __SA_DIMFMT_2D;
         _saStateCopy.ICNT0  = _overlapBufSize;
         _saStateCopy.ICNT1  = numChannels;
         _saStateCopy.DIM1   = static_cast<int32_t>(fftSize);

         _seTailRead.ELETYPE = c7x::se_eletype<vec>::value;
         _seTailRead.VECLEN  = c7x::se_veclen<vec>::value;
         _seTailRead.DIMFMT  = __SE_DIMFMT_2D;
         _seTailRead.ICNT0   = _overlapBufSize;
         _seTailRead.ICNT1   = numChannels;
         _seTailRead.DIM1    = static_cast<int32_t>(fftSize);

         _saTailWrite.VECLEN = c7x::sa_veclen<vec>::value;
         _saTailWrite.DIMFMT = __SA_DIMFMT_2D;
         _saTailWrite.ICNT0  = _overlapBufSize;
         _saTailWrite.ICNT1  = numChannels;
         _saTailWrite.DIM1   = static_cast<int32_t>(_overlapBufSize);

         _numStateVecs = numChannels * ((_overlapBufSize + vecElems - 1u) / vecElems);
      }
   }
}

template <typename T>
Istft<T>::Istft(T          *pX,
                T          *pY,
                T          *pWindow,
                T          *pOlaBuf,
                T          *pScratch,
                uint32_t    fftSize,
                uint32_t    hopSize,
                uint32_t    numChannels,
                uint32_t    numWindows,
                bool        onesided,
                const char *nodeName,
                uint32_t    nodeId,
                ErrorCtxt  &errorCtx)
    : Istft(pX, pY, pWindow, pOlaBuf, pScratch, fftSize, hopSize, numChannels, numWindows, onesided, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Istft<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Istft<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Istft<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Istft<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Istft<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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

template <typename T> void Istft<T>::execMultiWindow() noexcept
{
   for (uint32_t chIdx = 0; chIdx < _numChannels; chIdx++) {

      /*
       * step 1: batched inverse FFT for all numWindows frames of this channel.
       *
       * rebind the IFFT node's input to this channel's spectrum slice; the real
       * output lands in _pIFFTOut and _pFftScratch is used as butterfly scratch.
       */
      SetAddr_t ifftAddr;
      ifftAddr.src[0] = &_pX[chIdx * _numWindows * _inBins];
      _ifftNode->setAddr(ifftAddr, kAddrModeIn);
      _ifftNode->exec();

      /*
       * step 2: apply the synthesis window to all numWindows frames.
       *
       * SE0 reads _pIFFTOut (1D contiguous IFFT real output), SE1 reads _pWindow
       * (repeated numWindows times; DIM1=0 restarts), SA0 writes windowed frames
       * to _pFftScratch.  _pIFFTOut and _pFftScratch are non-overlapping.
       */
      T *restrict pWinDst   = _pFftScratch;
      int32_t numWindowVecs = static_cast<int32_t>(_numWindowVecs);

      __SE0_OPEN(_pIFFTOut, _seFrames);
      __SE1_OPEN(_pWindow, _seWindow);
      __SA0_OPEN(_saOutput);

      for (int32_t vecIdx = 0; vecIdx < numWindowVecs; vecIdx++) {
         vec     frameVec = c7x::strm_eng<0, vec>::get_adv();
         vec     winVec   = c7x::strm_eng<1, vec>::get_adv();
         vec     result   = frameVec * winVec;
         __vpred pred     = c7x::strm_agen<0, vec>::get_vpred();
         vec    *waddr    = c7x::strm_agen<0, vec>::get_adv(pWinDst);
         __vstore_pred(pred, waddr, result);
      }

      __SE0_CLOSE();
      __SE1_CLOSE();
      __SA0_CLOSE();

      /* step 3: overlap-add using long-accumulator scatter-add. */
      T *pOlaState          = &_pOlaBuf[chIdx * _overlapBufSize];
      T *restrict pOut      = &_pY[chIdx * _numOutputSamples];
      T *restrict pWorkBuf  = _pIFFTOut;
      T *restrict pEnvelope = _pEnvelope;

      /* 3.1 Init ONCE before the frame loop */
      memcpy(pWorkBuf, pOlaState, _overlapBufSize * sizeof(T));
      memset(pWorkBuf + _overlapBufSize, 0, _numOutputSamples * sizeof(T));

      /*
       * 3.2 Scatter-add each frame at its timeline offset.
       */
      T *restrict pScatterFrame = _pFftScratch;
      T *restrict pScatterAcc   = pWorkBuf;
      T       *pAccFrame        = pScatterAcc;
      uint32_t hopSize          = _hopSize;
      uint32_t numFrames        = _numWindows;
      uint32_t vecsPerFrame     = _scatterVecsPerFrame;

      __SE0_OPEN(pScatterFrame, _seScatterFrame);
      __SE1_OPEN(pScatterAcc, _seScatterAcc);
      __SA0_OPEN(_saScatter);

      for (uint32_t frameIdx = 0; frameIdx < numFrames; frameIdx++) {
         __SE1_RESET(pAccFrame);
         pAccFrame += hopSize;

         for (uint32_t vecIdx = 0; vecIdx < vecsPerFrame; vecIdx++) {
            vec     frameVec = c7x::strm_eng<0, vec>::get_adv();
            vec     accVec   = c7x::strm_eng<1, vec>::get_adv();
            __vpred pred     = c7x::strm_agen<0, vec>::get_vpred();
            vec    *olaAddr  = c7x::strm_agen<0, vec>::get_adv(pScatterAcc);
            __vstore_pred(pred, olaAddr, accVec + frameVec);
         }
      }

      __SE0_CLOSE();
      __SE1_CLOSE();
      __SA0_CLOSE();

      /* 3.3 Emit output with envelope normalization. */
      uint32_t numEmitVecs = _numEmitVecs;

      __SE0_OPEN(pWorkBuf, _seEmitWork);
      __SE1_OPEN(pEnvelope, _seEmitEnv);
      __SA0_OPEN(_saEmit);

      for (uint32_t vecIdx = 0; vecIdx < numEmitVecs; vecIdx++) {
         vec     workVec = c7x::strm_eng<0, vec>::get_adv();
         vec     envVec  = c7x::strm_eng<1, vec>::get_adv();
         __vpred pred    = c7x::strm_agen<0, vec>::get_vpred();
         vec    *outAddr = c7x::strm_agen<0, vec>::get_adv(pOut);
         __vstore_pred(pred, outAddr, workVec * envVec);
      }

      __SE0_CLOSE();
      __SE1_CLOSE();
      __SA0_CLOSE();

      /* 4. Save OLA state ONCE after the frame loop (un-normalized raw accumulator) */
      memcpy(pOlaState, pWorkBuf + _numOutputSamples, _overlapBufSize * sizeof(T));
   }
}

template <typename T> void Istft<T>::execBatchChannels() noexcept
{
   /* step 1: one inverse FFT batched across all channels (_pX is [numChannels x inBins]
    * contiguous) -> [numChannels x fftSize] in _pIFFTOut. */
   SetAddr_t ifftAddr;
   ifftAddr.src[0] = _pX;
   _ifftNode->setAddr(ifftAddr, kAddrModeIn);
   _ifftNode->exec();

   /* step 2: window all channels in a single pass -> windowed frames in _pFftScratch. */
   T *restrict pWinDst   = _pFftScratch;
   int32_t numWindowVecs = static_cast<int32_t>(_numWindowVecs);

   __SE0_OPEN(_pIFFTOut, _seFrames);
   __SE1_OPEN(_pWindow, _seWindow);
   __SA0_OPEN(_saOutput);

   for (int32_t vecIdx = 0; vecIdx < numWindowVecs; vecIdx++) {
      vec     frameVec = c7x::strm_eng<0, vec>::get_adv();
      vec     winVec   = c7x::strm_eng<1, vec>::get_adv();
      vec     result   = frameVec * winVec;
      __vpred pred     = c7x::strm_agen<0, vec>::get_vpred();
      vec    *waddr    = c7x::strm_agen<0, vec>::get_adv(pWinDst);
      __vstore_pred(pred, waddr, result);
   }

   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();

   /* step 3: overlap-add = (previous tail) + (windowed frame), no intra-block overlap.
    * workbuf = _pIFFTOut (IFFT output consumed by step 2); windowed frames in _pFftScratch. */
   T *restrict pWorkBuf  = _pIFFTOut;
   T *restrict pWindowed = _pFftScratch;
   T *restrict pOut      = _pY;
   T *restrict pEnvelope = _pEnvelope;

   /* 3.1 preload: zero workbuf, then scatter the previous tail into each channel's overlap region. */
   memset(pWorkBuf, 0, static_cast<size_t>(_numChannels) * _fftSize * sizeof(T));

   uint32_t numStateVecs = _numStateVecs;

   __SE0_OPEN(_pOlaBuf, _seStateCopy);
   __SA0_OPEN(_saStateCopy);

   for (uint32_t vecIdx = 0; vecIdx < numStateVecs; vecIdx++) {
      vec     stateVec = c7x::strm_eng<0, vec>::get_adv();
      __vpred pred     = c7x::strm_agen<0, vec>::get_vpred();
      vec    *addr     = c7x::strm_agen<0, vec>::get_adv(pWorkBuf);
      __vstore_pred(pred, addr, stateVec);
   }

   __SE0_CLOSE();
   __SA0_CLOSE();

   /* 3.2 add: workbuf[i] += windowed[i] over numChannels*fftSize (no overlap, flat add). */
   uint32_t numScatterVecs = _numScatterVecs;

   __SE0_OPEN(pWindowed, _seScatterFrame);
   __SE1_OPEN(pWorkBuf, _seScatterAcc);
   __SA0_OPEN(_saScatter);

   for (uint32_t vecIdx = 0; vecIdx < numScatterVecs; vecIdx++) {
      vec     frameVec = c7x::strm_eng<0, vec>::get_adv();
      vec     accVec   = c7x::strm_eng<1, vec>::get_adv();
      __vpred pred     = c7x::strm_agen<0, vec>::get_vpred();
      vec    *addr     = c7x::strm_agen<0, vec>::get_adv(pWorkBuf);
      __vstore_pred(pred, addr, accVec + frameVec);
   }

   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();

   /* 3.3 emit: out[ch*hop + s] = workbuf[ch*fftSize + s] * env[s], s in [0,hopSize). */
   uint32_t numEmitVecs = _numEmitVecs;

   __SE0_OPEN(pWorkBuf, _seEmitWork);
   __SE1_OPEN(pEnvelope, _seEmitEnv);
   __SA0_OPEN(_saEmit);

   for (uint32_t vecIdx = 0; vecIdx < numEmitVecs; vecIdx++) {
      vec     workVec = c7x::strm_eng<0, vec>::get_adv();
      vec     envVec  = c7x::strm_eng<1, vec>::get_adv();
      __vpred pred    = c7x::strm_agen<0, vec>::get_vpred();
      vec    *outAddr = c7x::strm_agen<0, vec>::get_adv(pOut);
      __vstore_pred(pred, outAddr, workVec * envVec);
   }

   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();

   /* 4. save the new tail per channel: workbuf[ch*fftSize + hopSize ..] -> pOlaBuf. */
   uint32_t numTailVecs = _numStateVecs;
   T *restrict pTailDst = _pOlaBuf;

   __SE0_OPEN(&pWorkBuf[_hopSize], _seTailRead);
   __SA0_OPEN(_saTailWrite);

   for (uint32_t vecIdx = 0; vecIdx < numTailVecs; vecIdx++) {
      __vpred pred = c7x::strm_agen<0, vec>::get_vpred();
      vec    *addr = c7x::strm_agen<0, vec>::get_adv(pTailDst);
      __vstore_pred(pred, addr, c7x::strm_eng<0, vec>::get_adv());
   }

   __SE0_CLOSE();
   __SA0_CLOSE();
}

template <typename T> uint32_t Istft<T>::getOlaBufSize() const noexcept { return _overlapBufSize; }

template <typename T> uint32_t Istft<T>::getNumOutputSamples() const noexcept { return _numOutputSamples; }

template <typename T> uint32_t Istft<T>::calcOlaBufSize(uint32_t fftSize, uint32_t hopSize) noexcept
{
   return fftSize - hopSize;
}

template <typename T> uint32_t Istft<T>::calcNumOutputSamples(uint32_t numWindows, uint32_t hopSize) noexcept
{
   return numWindows * hopSize;
}

template <typename T>
uint32_t Istft<T>::calcScratchSize(uint32_t fftSize, uint32_t numWindows, uint32_t numChannels) noexcept
{
   /* when numWindows==1 and numChannels>1, the inverse FFT is batched across channels,
    * so the scratch must be sized for batchSize = numChannels frames instead of numWindows. */
   uint32_t batchSize = ((numWindows == 1u) && (numChannels > 1u)) ? numChannels : numWindows;

   constexpr uint32_t alignElems    = static_cast<uint32_t>(TISP_SE_SA_ALIGN_BYTES / sizeof(T));
   uint32_t           firstHalfSize = ((batchSize * fftSize) + alignElems - 1u) & ~(alignElems - 1u);
   /* second half doubles as the IFFTR1dBatched butterfly scratch, which FFTLIB
    * requires to be batchSize*(fftSize+1)*2 floats — larger than the windowed
    * frame buffer (batchSize*fftSize), so size for the FFT scratch. */
   uint32_t secondHalfSize = batchSize * (fftSize + 1u) * 2u;
   /* third region holds the steady-state window envelope (hopSize entries,
    * hopSize <= fftSize); reserve fftSize since hopSize is not passed here. */
   uint32_t envelopeSize = fftSize;
   return firstHalfSize + secondHalfSize + envelopeSize;
}

template <typename T>
float Istft<T>::calcTotalMemKB(uint32_t fftSize,
                               uint32_t hopSize,
                               uint32_t numChannels,
                               uint32_t numWindows,
                               bool     onesided) noexcept
{
   uint32_t inBins           = onesided ? (fftSize + 2u) : (2u * fftSize);
   uint32_t overlapBufSize   = calcOlaBufSize(fftSize, hopSize);
   uint32_t numOutputSamples = calcNumOutputSamples(numWindows, hopSize);
   uint32_t scratchElems     = calcScratchSize(fftSize, numWindows, numChannels);

   uint32_t totalElems = (numChannels * numWindows * inBins) + (numChannels * numOutputSamples) + fftSize +
                         (overlapBufSize * numChannels) + scratchElems;

   return static_cast<float>(totalElems * sizeof(T)) / 1024.0f;
}

template <typename T>
void Istft<T>::cleanInternalBuffers() noexcept
{
   std::memset(_pOlaBuf, 0, _historyBufSize);
}

/***************************/
/* Explicit instantiation  */
/***************************/

template class Istft<float>;

} /* namespace AUDIOLIB */
} /* namespace TISP */
