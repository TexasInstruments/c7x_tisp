// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_stft.hpp>
#include <cstdint>
#include <cstring>
#include <memory>

namespace TISP {
namespace AUDIOLIB {

template <typename T>
Stft<T>::Stft(T        *pX,
              T        *pY,
              T        *pWindow,
              T        *pState,
              T        *pScratch,
              uint32_t  fftSize,
              uint32_t  hopSize,
              uint32_t  numChannels,
              uint32_t  numInputSamples,
              bool      onesided,
              ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY), _pWindow(pWindow), _pState(pState), _pScratch(pScratch), _fftSize(fftSize),
      _hopSize(hopSize), _numChannels(numChannels), _numInputSamples(numInputSamples), _onesided(onesided)
{
   _outBins = onesided ? (fftSize + 2u) : (2u * fftSize);

   /* pWindow must be a valid fftSize-length buffer; when the analysis window is     */
   /* shorter than fftSize, the caller is responsible for producing a correctly      */
   /* zero-padded buffer via the windowGen overload. The node cannot verify contents.*/

   /* fill params first so a node that fails validation still reports its configuration */
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "fftSize=%u,hopSize=%u,numCh=%u,numInputSamples=%u,onesided=%s", fftSize, hopSize, numChannels,
                   numInputSamples, onesided ? "true" : "false");

   /* input validation */
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Stft constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("Stft constructor", "output pointer pY is null");
   }
   if (pWindow == nullptr) {
      errorCtx.setNullPointerError("Stft constructor", "window pointer pWindow is null");
   }
   if (pScratch == nullptr) {
      errorCtx.setNullPointerError("Stft constructor", "scratch pointer pScratch is null");
   }
   if (fftSize == 0u) {
      errorCtx.setInvalidParameterError("Stft constructor", "fftSize must be greater than zero");
   }
   if (hopSize == 0u) {
      errorCtx.setInvalidParameterError("Stft constructor", "hopSize must be greater than zero");
   }
   else if (hopSize > fftSize) {
      errorCtx.setInvalidParameterError("Stft constructor", "hopSize must not exceed fftSize");
   }
   if (numChannels == 0u) {
      errorCtx.setInvalidParameterError("Stft constructor", "numChannels must be greater than zero");
   }
   if (numInputSamples == 0u) {
      errorCtx.setInvalidParameterError("Stft constructor", "numInputSamples must be greater than zero");
   }
   else if ((hopSize != 0u) && (numInputSamples < hopSize)) {
      errorCtx.setInvalidParameterError("Stft constructor", "numInputSamples must be >= hopSize");
   }
   if ((hopSize != 0u) && ((numInputSamples % hopSize) != 0u)) {
      errorCtx.setInvalidParameterError("Stft constructor", "numInputSamples must be a multiple of hopSize");
   }

   /* derived constants — computed only after fftSize/hopSize are validated, so the
    * calc helpers cannot divide by zero or underflow on (numInputSamples - fftSize) */
   if (errorCtx.isSuccess()) {
      _numWindows     = calcNumWindows(fftSize, hopSize, numInputSamples);
      _overlapBufSize = calcOverlapBufSize(fftSize, hopSize, numInputSamples);

      if ((pState == nullptr) && (_overlapBufSize != 0u)) {
         errorCtx.setNullPointerError("Stft constructor", "state pointer pState is null");
      }
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = _numInputSamples;
      _nodeInfo.bufParams.heightIn[0]       = _numChannels;
      _nodeInfo.bufParams.inPitchBytes[0]   = _numInputSamples * sizeof(T);

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = _numWindows * _outBins;
      _nodeInfo.bufParams.heightOut[0]       = _numChannels;
      _nodeInfo.bufParams.outPitchBytes[0]   = _numWindows * _outBins * sizeof(T);

      /*
       * two-pass windowing split for execMultiWindow optimization.
       *
       * instead of copying ALL F input samples into a staging buffer, we only
       * copy enough to "bridge" the state|input boundary. the remaining windows
       * read directly from _pX with zero copy overhead.
       *
       * three cases based on overlap and input size:
       *
       * case 1 (F >= N, S > 0): two-pass. numBoundary = ceil(S/H) windows read
       *   from staging (state + bridge), numDirect = numWindows - numBoundary
       *   windows read directly from _pX. the bridge copies only numBoundary * H
       *   input samples instead of all F.
       *   proof that numDirect >= 1: when F >= N and F%H==0, S is always a
       *   multiple of H, so numBoundary = S/H = ceil(N/H) - 1. since
       *   F/H >= ceil(N/H), numDirect = F/H - (ceil(N/H) - 1) >= 1.
       *   proof that tail is in _pX: numWindows * H = F (precondition F%H==0),
       *   so the last window ends at _pX[F-1]. tail = _pX[F-S..F-1].
       *
       * case 2 (S == 0): zero-copy. all windows read directly from _pX.
       *   no state to prepend, no tail to save.
       *
       * case 3 (F < N, S > 0): sub-window. all windows are boundary windows.
       *   the full input is copied to staging (same as unoptimized behavior).
       */
      if ((_overlapBufSize > 0u) && (numInputSamples >= fftSize)) {
         _numBoundaryWindows = (_overlapBufSize + hopSize - 1u) / hopSize;
         _numDirectWindows   = _numWindows - _numBoundaryWindows;
         _bridgeSize         = _numBoundaryWindows * hopSize;
         _directStartInInput = _numBoundaryWindows * hopSize - _overlapBufSize;
      }
      else if (_overlapBufSize == 0u) {
         _numBoundaryWindows = 0u;
         _numDirectWindows   = _numWindows;
         _bridgeSize         = 0u;
         _directStartInInput = 0u;
      }
      else {
         /* sub-window case (F < fftSize): all windows need staging */
         _numBoundaryWindows = _numWindows;
         _numDirectWindows   = 0u;
         _bridgeSize         = numInputSamples;
         _directStartInInput = 0u;
      }

      /* zero-initialize overlap state so first exec() sees silence in the tail */
      _historyBufSize = static_cast<size_t>(_overlapBufSize) * numChannels;
      cleanInternalBuffers();

      /*
       * when numWindows==1 and numChannels>1, batch the FFT across channels
       * instead of looping per channel.  batchSize drives scratch layout,
       * RFFT batch count, and SE/SA template configuration.
       */
      uint32_t batchSize = _numWindows;
      if ((_numWindows == 1u) && (numChannels > 1u)) {
         batchSize = numChannels;
         _execImpl = &Stft::execBatchChannels;
      }
      else {
         _execImpl = &Stft::execMultiWindow;
      }

      /* split caller's scratch buffer; round up first half to SE/SA alignment boundary */
      constexpr uint32_t alignElems     = TISP_SE_SA_ALIGN_BYTES / sizeof(T);
      uint32_t           firstHalfElems = ((batchSize * fftSize) + alignElems - 1u) & ~(alignElems - 1u);
      _pFftScratch                      = &pScratch[firstHalfElems];

      const TISP_BufDesc2D   rfftIn      = {_pScratch, ElementDType<T>::value, fftSize, batchSize,
                                             static_cast<size_t>(fftSize) * sizeof(T), 0U};
      const TISP_BufDesc2D   rfftOut     = {pY, ElementDType<T>::value, _outBins, batchSize,
                                             static_cast<size_t>(_outBins) * sizeof(T), 0U};
      const TISP_BufDescList rfftInputs  = {&rfftIn, 1U};
      const TISP_BufDescList rfftOutputs = {&rfftOut, 1U};

      TISP::FFTLIB::RFFT1dBatchedConfig rfftBuild;
      rfftBuild.fullSpectrum = !onesided;

      _rfftNode = TISP::make_unique_node<TISP::FFTLIB::RFFT1dBatched<T>>(
          &rfftInputs, &rfftOutputs, _pFftScratch, static_cast<size_t>(batchSize) * fftSize * sizeof(T), &rfftBuild,
          "RFFT1dBatched", 0U, errorCtx);
      /* make_unique_node returns null on L2 node-arena exhaustion without touching
       * errorCtx, so a null result must be reported explicitly as a memory error */
      if (_rfftNode == nullptr) {
         errorCtx.setMemoryError("Stft constructor", "failed to allocate RFFT1dBatched node from L2 node arena");
      }

      /* block copy nodes for C7x optimized data movement (SE/SA) */
      if (errorCtx.isSuccess() && (_overlapBufSize > 0u)) {
         _blockCopyOverlap =
             TISP::make_unique_node<TISP::misc::BlockCopy<T>>(pState, _pFftScratch, _overlapBufSize, 1u, errorCtx);
         if (_blockCopyOverlap == nullptr) {
            errorCtx.setMemoryError("Stft constructor", "failed to allocate overlap BlockCopy node from L2 node arena");
         }
      }
      /* bridge copy: only _bridgeSize input samples (not all F) for multi-window path */
      if (errorCtx.isSuccess() && (_bridgeSize > 0u)) {
         _blockCopyInput = TISP::make_unique_node<TISP::misc::BlockCopy<T>>(pX, &_pFftScratch[_overlapBufSize],
                                                                            _bridgeSize, 1u, errorCtx);
         if (_blockCopyInput == nullptr) {
            errorCtx.setMemoryError("Stft constructor", "failed to allocate input BlockCopy node from L2 node arena");
         }
      }

      /* SE/SA template initialization — delegated to per-member init functions */
      initSeStaging();
      initSeWindow();
      initSaOutput();

      uint32_t vecElems = c7x::max_simd<T>::value;
      if ((_numWindows == 1u) && (_numChannels > 1u)) {
         _numWindowVecs = (_numChannels * _fftSize + vecElems - 1u) / vecElems;
      }
      else if (_numBoundaryWindows > 0u) {
         _numBoundaryVecs = (_numBoundaryWindows * _fftSize + vecElems - 1u) / vecElems;
      }

      if ((_numWindows == 1u) && (_numChannels > 1u) && (_overlapBufSize > 0u)) {
         initSeStateCopy();
         initSaStateCopy();
         initSeInputCopy();
         initSaInputCopy();
         initSeTailRead();
         initSaTailWrite();

         int32_t eleCount  = c7x::element_count_of<vec>::value;
         _numCopyStateVecs = (_overlapBufSize * _numChannels + static_cast<uint32_t>(eleCount) - 1u) /
                             static_cast<uint32_t>(eleCount);
         _numCopyInputVecs = (_numInputSamples * _numChannels + static_cast<uint32_t>(eleCount) - 1u) /
                             static_cast<uint32_t>(eleCount);
      }

      if (_numDirectWindows > 0u) {
         initSeStagingDirect();
         initSeWindowDirect();
         initSaOutputDirect();
         _numDirectVecs = (_numDirectWindows * _fftSize + vecElems - 1u) / vecElems;
      }
   }
}

template <typename T>
Stft<T>::Stft(T          *pX,
              T          *pY,
              T          *pWindow,
              T          *pState,
              T          *pScratch,
              uint32_t    fftSize,
              uint32_t    hopSize,
              uint32_t    numChannels,
              uint32_t    numInputSamples,
              bool        onesided,
              const char *nodeName,
              uint32_t    nodeId,
              ErrorCtxt  &errorCtx)
    : Stft(pX, pY, pWindow, pState, pScratch, fftSize, hopSize, numChannels, numInputSamples, onesided, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

/* stft setAddr */

template <typename T> void *Stft<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *Stft<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t Stft<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t Stft<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void Stft<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
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


/* =========================================================================
 * SE/SA template initializers
 * ========================================================================= */

/*
 * windowing SE/SA — used by both execMultiWindow (boundary pass) and
 * execBatchChannels. called unconditionally; branches on _numWindows /
 * _numBoundaryWindows to set the path-specific ICNT1 / DIM1 fields.
 */
template <typename T>
void Stft<T>::initSeStaging() noexcept
{
   _seStaging.ELETYPE = c7x::se_eletype<vec>::value;
   _seStaging.VECLEN  = c7x::se_veclen<vec>::value;
   _seStaging.DIMFMT  = __SE_DIMFMT_2D;
   _seStaging.ICNT0   = _fftSize;

   if ((_numWindows == 1u) && (_numChannels > 1u)) {
      _seStaging.ICNT1 = _numChannels;
      _seStaging.DIM1  = static_cast<int32_t>(_fftSize);
   }
   else if (_numBoundaryWindows > 0u) {
      _seStaging.ICNT1 = _numBoundaryWindows;
      _seStaging.DIM1  = static_cast<int32_t>(_hopSize);
   }
}

template <typename T>
void Stft<T>::initSeWindow() noexcept
{
   _seWindow.ELETYPE = c7x::se_eletype<vec>::value;
   _seWindow.VECLEN  = c7x::se_veclen<vec>::value;
   _seWindow.DIMFMT  = __SE_DIMFMT_2D;
   _seWindow.ICNT0   = _fftSize;
   _seWindow.DIM1    = 0;

   if ((_numWindows == 1u) && (_numChannels > 1u)) {
      _seWindow.ICNT1 = _numChannels;
   }
   else if (_numBoundaryWindows > 0u) {
      _seWindow.ICNT1 = _numBoundaryWindows;
   }
}

template <typename T>
void Stft<T>::initSaOutput() noexcept
{
   _saOutput.VECLEN = c7x::sa_veclen<vec>::value;
   _saOutput.DIMFMT = __SA_DIMFMT_1D;

   if ((_numWindows == 1u) && (_numChannels > 1u)) {
      _saOutput.ICNT0 = _numChannels * _fftSize;
   }
   else if (_numBoundaryWindows > 0u) {
      _saOutput.ICNT0 = _numBoundaryWindows * _fftSize;
   }
}

/*
 * batch-channels copy SE/SA — three passes: state->staging, input->staging,
 * tail save staging->state. called only when (_numWindows==1) && (numCh>1)
 * && (_overlapBufSize>0).
 */
template <typename T>
void Stft<T>::initSeStateCopy() noexcept
{
   _seStateCopy.ELETYPE = c7x::se_eletype<vec>::value;
   _seStateCopy.VECLEN  = c7x::se_veclen<vec>::value;
   _seStateCopy.DIMFMT  = __SE_DIMFMT_1D;
   _seStateCopy.ICNT0   = _overlapBufSize * _numChannels;
}

template <typename T>
void Stft<T>::initSaStateCopy() noexcept
{
   _saStateCopy.VECLEN = c7x::sa_veclen<vec>::value;
   _saStateCopy.DIMFMT = __SA_DIMFMT_2D;
   _saStateCopy.ICNT0  = _overlapBufSize;
   _saStateCopy.DIM1   = static_cast<int32_t>(_fftSize);
   _saStateCopy.ICNT1  = _numChannels;
}

template <typename T>
void Stft<T>::initSeInputCopy() noexcept
{
   _seInputCopy.ELETYPE = c7x::se_eletype<vec>::value;
   _seInputCopy.VECLEN  = c7x::se_veclen<vec>::value;
   _seInputCopy.DIMFMT  = __SE_DIMFMT_1D;
   _seInputCopy.ICNT0   = _numInputSamples * _numChannels;
}

template <typename T>
void Stft<T>::initSaInputCopy() noexcept
{
   _saInputCopy.VECLEN = c7x::sa_veclen<vec>::value;
   _saInputCopy.DIMFMT = __SA_DIMFMT_2D;
   _saInputCopy.ICNT0  = _numInputSamples;
   _saInputCopy.DIM1   = static_cast<int32_t>(_fftSize);
   _saInputCopy.ICNT1  = _numChannels;
}

template <typename T>
void Stft<T>::initSeTailRead() noexcept
{
   _seTailRead.ELETYPE = c7x::se_eletype<vec>::value;
   _seTailRead.VECLEN  = c7x::se_veclen<vec>::value;
   _seTailRead.DIMFMT  = __SE_DIMFMT_2D;
   _seTailRead.ICNT0   = _overlapBufSize;
   _seTailRead.DIM1    = static_cast<int32_t>(_fftSize);
   _seTailRead.ICNT1   = _numChannels;
}

template <typename T>
void Stft<T>::initSaTailWrite() noexcept
{
   _saTailWrite.VECLEN = c7x::sa_veclen<vec>::value;
   _saTailWrite.DIMFMT = __SA_DIMFMT_1D;
   _saTailWrite.ICNT0  = _overlapBufSize * _numChannels;
}

/*
 * direct-window SE/SA — used by execMultiWindow pass 2 for windows that lie
 * entirely within _pX. called only when _numDirectWindows > 0.
 */
template <typename T>
void Stft<T>::initSeStagingDirect() noexcept
{
   _seStagingDirect.ELETYPE = c7x::se_eletype<vec>::value;
   _seStagingDirect.VECLEN  = c7x::se_veclen<vec>::value;
   _seStagingDirect.DIMFMT  = __SE_DIMFMT_2D;
   _seStagingDirect.ICNT0   = _fftSize;
   _seStagingDirect.ICNT1   = _numDirectWindows;
   _seStagingDirect.DIM1    = static_cast<int32_t>(_hopSize);
}

template <typename T>
void Stft<T>::initSeWindowDirect() noexcept
{
   _seWindowDirect.ELETYPE = c7x::se_eletype<vec>::value;
   _seWindowDirect.VECLEN  = c7x::se_veclen<vec>::value;
   _seWindowDirect.DIMFMT  = __SE_DIMFMT_2D;
   _seWindowDirect.ICNT0   = _fftSize;
   _seWindowDirect.ICNT1   = _numDirectWindows;
   _seWindowDirect.DIM1    = 0;
}

template <typename T>
void Stft<T>::initSaOutputDirect() noexcept
{
   _saOutputDirect.VECLEN = c7x::sa_veclen<vec>::value;
   _saOutputDirect.DIMFMT = __SA_DIMFMT_1D;
   _saOutputDirect.ICNT0  = _numDirectWindows * _fftSize;
}


template <typename T> void Stft<T>::execMultiWindow() noexcept
{
   for (uint32_t chIdx = 0; chIdx < _numChannels; chIdx++) {

      T *restrict pScratchLocal = _pScratch;

      /*
       * step 1: boundary pass — window frames that straddle state|input.
       *
       * only boundary windows need a staging buffer. we copy the overlap
       * state (S elements) + a bridge portion of the input (bridgeSize
       * elements) into _pFftScratch, then window _numBoundaryWindows
       * frames from there.
       *
       * bridgeSize = numBoundary * hopSize, which is much smaller than F
       * for large-input configs. example (test 17, N=512, H=160, F=16000):
       *   boundary = 3, bridge = 480 vs current copy of all F = 16000.
       *   saves 60.6 KB per channel.
       */
      if (_numBoundaryWindows > 0u) {
         if (_overlapBufSize > 0u) {
            SetAddr_t copyAddr;
            copyAddr.src[0] = &_pState[chIdx * _overlapBufSize];
            copyAddr.dst[0] = _pFftScratch;
            _blockCopyOverlap->setAddr(copyAddr, kAddrModeInOut);
            _blockCopyOverlap->exec();
         }

         if (_bridgeSize > 0u) {
            SetAddr_t copyAddr;
            copyAddr.src[0] = &_pX[chIdx * _numInputSamples];
            copyAddr.dst[0] = &_pFftScratch[_overlapBufSize];
            _blockCopyInput->setAddr(copyAddr, kAddrModeInOut);
            _blockCopyInput->exec();
         }

         /* SE0: hop-strided reads from staging, SE1: window (restarts), SA0: flat output */
         int32_t numBoundaryVecs = static_cast<int32_t>(_numBoundaryVecs);

         __SE0_OPEN(_pFftScratch, _seStaging);
         __SE1_OPEN(_pWindow, _seWindow);
         __SA0_OPEN(_saOutput);

         for (int32_t vecIdx = 0; vecIdx < numBoundaryVecs; vecIdx++) {
            vec     inVec  = c7x::strm_eng<0, vec>::get_adv();
            vec     winVec = c7x::strm_eng<1, vec>::get_adv();
            vec     result = inVec * winVec;
            __vpred pred   = c7x::strm_agen<0, vec>::get_vpred();
            vec    *addr   = c7x::strm_agen<0, vec>::get_adv(pScratchLocal);
            __vstore_pred(pred, addr, result);
         }

         __SE0_CLOSE();
         __SE1_CLOSE();
         __SA0_CLOSE();
      }

      /*
       * step 2: direct pass — window frames entirely within _pX (zero copy).
       *
       * these windows start at _pX[directStartInInput] and advance by
       * hopSize per window. no data is copied — SE reads directly from
       * the caller's input buffer.
       *
       * directStartInInput = numBoundary * hopSize - S. this is the offset
       * into _pX where the first direct window's fftSize-element read range
       * begins entirely within the input (no state contribution).
       *
       * the windowed output is written contiguously after the boundary
       * output in _pScratch (offset = numBoundary * fftSize).
       */
      if (_numDirectWindows > 0u) {
         T *pDirectSrc          = &_pX[chIdx * _numInputSamples + _directStartInInput];
         T *restrict pDirectDst = _pScratch + _numBoundaryWindows * _fftSize;

         int32_t numDirectVecs = static_cast<int32_t>(_numDirectVecs);

         __SE0_OPEN(pDirectSrc, _seStagingDirect);
         __SE1_OPEN(_pWindow, _seWindowDirect);
         __SA0_OPEN(_saOutputDirect);

         for (int32_t vecIdx = 0; vecIdx < numDirectVecs; vecIdx++) {
            vec     inVec  = c7x::strm_eng<0, vec>::get_adv();
            vec     winVec = c7x::strm_eng<1, vec>::get_adv();
            vec     result = inVec * winVec;
            __vpred pred   = c7x::strm_agen<0, vec>::get_vpred();
            vec    *addr   = c7x::strm_agen<0, vec>::get_adv(pDirectDst);
            __vstore_pred(pred, addr, result);
         }

         __SE0_CLOSE();
         __SE1_CLOSE();
         __SA0_CLOSE();
      }

      /*
       * step 3: save the unconsumed tail for the next exec() call.
       *
       * when direct windows exist (case 1, F >= N): tail = last S elements
       * of this channel's _pX. proof: numWindows * hopSize = F (precondition
       * F%H==0), so the last window ends at _pX[F-1]. the unconsumed tail
       * is _pX[F-S .. F-1], which has exactly S = overlapBufSize elements.
       * F > S is guaranteed since F >= N > N-H = S.
       *
       * when no direct windows (case 3, sub-window F < N): tail is in the
       * staging buffer at offset numWindows * hopSize, same as original path.
       */
      if (_overlapBufSize > 0u) {
         SetAddr_t copyAddr;
         if (_numDirectWindows > 0u) {
            /* tail from _pX: last S elements of this channel's input */
            copyAddr.src[0] = &_pX[chIdx * _numInputSamples + _numInputSamples - _overlapBufSize];
         }
         else {
            /* sub-window: tail still in staging buffer */
            copyAddr.src[0] = &_pFftScratch[_numWindows * _hopSize];
         }
         copyAddr.dst[0] = &_pState[chIdx * _overlapBufSize];
         _blockCopyOverlap->setAddr(copyAddr, kAddrModeInOut);
         _blockCopyOverlap->exec();
      }

      /*
       * step 4: batched fft
       *
       * _pScratch (windowed frames) -> _pFftScratch (FFT scratch) -> _pY.
       * _pFftScratch staging data is fully consumed above, safe to overwrite
       * as FFT scratch. point the rfft output to this channel's slice of _pY.
       */
      SetAddr_t rfftAddr;
      rfftAddr.dst[0] = &_pY[chIdx * _numWindows * _outBins];
      _rfftNode->setAddr(rfftAddr, kAddrModeOut);
      _rfftNode->exec();
   }
}

template <typename T> void Stft<T>::execBatchChannels() noexcept
{
   T *pWindowSrc = _pX; /* default: window directly from input (overlap==0) */

   if (_overlapBufSize > 0u) {
      /*
       * build staging for all channels in _pFftScratch:
       * [ch0: overlapBufSize + numInputSamples][ch1: ...][chN: ...]
       * each channel occupies fftSize contiguous floats at stride fftSize.
       *
       * SE reads contiguous source data (1D), SA writes with 2D stride to
       * place each channel's chunk at the correct fftSize offset — one
       * SE open/close per copy instead of numChannels BlockCopy calls.
       */
      T *restrict pDst = _pFftScratch;

      /* pass 1: copy state for all channels (contiguous -> scattered) */
      int32_t numCopyStateVecs = static_cast<int32_t>(_numCopyStateVecs);

      __SE0_OPEN(_pState, _seStateCopy);
      __SA0_OPEN(_saStateCopy);
      for (int32_t vecIdx = 0; vecIdx < numCopyStateVecs; vecIdx++) {
         __vpred pred = c7x::strm_agen<0, vec>::get_vpred();
         vec    *addr = c7x::strm_agen<0, vec>::get_adv(pDst);
         __vstore_pred(pred, addr, c7x::strm_eng<0, vec>::get_adv());
      }
      __SE0_CLOSE();
      __SA0_CLOSE();

      /* pass 2: copy input for all channels (contiguous -> scattered, offset) */
      T *restrict pDstInput  = &_pFftScratch[_overlapBufSize];
      int32_t     numCopyInputVecs = static_cast<int32_t>(_numCopyInputVecs);

      __SE0_OPEN(_pX, _seInputCopy);
      __SA0_OPEN(_saInputCopy);
      for (int32_t vecIdx = 0; vecIdx < numCopyInputVecs; vecIdx++) {
         __vpred pred = c7x::strm_agen<0, vec>::get_vpred();
         vec    *addr = c7x::strm_agen<0, vec>::get_adv(pDstInput);
         __vstore_pred(pred, addr, c7x::strm_eng<0, vec>::get_adv());
      }
      __SE0_CLOSE();
      __SA0_CLOSE();

      pWindowSrc = _pFftScratch;
   }

   /*
    * window ALL channels in one SE pass
    * SE0: fftSize per channel, numChannels batches, stride fftSize
    * SE1: same window restarted per channel (DIM1=0)
    * SA0: flat write of numChannels * fftSize into _pScratch
    */
   T      *restrict pScratchLocal = _pScratch;
   int32_t numWindowVecs          = static_cast<int32_t>(_numWindowVecs);

   __SE0_OPEN(pWindowSrc, _seStaging);
   __SE1_OPEN(_pWindow, _seWindow);
   __SA0_OPEN(_saOutput);

   for (int32_t vecIdx = 0; vecIdx < numWindowVecs; vecIdx++) {
      vec     inVec  = c7x::strm_eng<0, vec>::get_adv();
      vec     winVec = c7x::strm_eng<1, vec>::get_adv();
      vec     result = inVec * winVec;
      __vpred pred   = c7x::strm_agen<0, vec>::get_vpred();
      vec    *addr   = c7x::strm_agen<0, vec>::get_adv(pScratchLocal);
      __vstore_pred(pred, addr, result);
   }

   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();

   /* pass 3: tail save for all channels (scattered -> contiguous) */
   if (_overlapBufSize > 0u) {
      T *restrict pTailSrc    = &_pFftScratch[_numWindows * _hopSize];
      T *restrict pStateDst   = _pState;
      int32_t     numTailVecs = static_cast<int32_t>(_numCopyStateVecs);

      __SE0_OPEN(pTailSrc, _seTailRead);
      __SA0_OPEN(_saTailWrite);
      for (int32_t vecIdx = 0; vecIdx < numTailVecs; vecIdx++) {
         __vpred pred = c7x::strm_agen<0, vec>::get_vpred();
         vec    *addr = c7x::strm_agen<0, vec>::get_adv(pStateDst);
         __vstore_pred(pred, addr, c7x::strm_eng<0, vec>::get_adv());
      }
      __SE0_CLOSE();
      __SA0_CLOSE();
   }

   /* single batched FFT across all channels — rebind output in case setAddr() moved _pY */
   SetAddr_t rfftAddr;
   rfftAddr.dst[0] = _pY;
   _rfftNode->setAddr(rfftAddr, kAddrModeOut);
   _rfftNode->exec();
}

template <typename T> uint32_t Stft<T>::getNumWindows() const noexcept { return _numWindows; }

template <typename T> uint32_t Stft<T>::getOverlapBufSize() const noexcept { return _overlapBufSize; }

template <typename T>
uint32_t Stft<T>::calcOverlapBufSize(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples) noexcept
{
   uint32_t overlapBufSize = 0u;

   /* hopSize == 0 leaves overlapBufSize at 0: guards standalone callers that skip constructor validation */
   if (hopSize != 0u) {
      if (numInputSamples < fftSize) {
         /* input block smaller than FFT window — overlap must bridge the gap */
         overlapBufSize = fftSize - hopSize;
      }
      else {
         uint32_t firstFrameWindows = (numInputSamples - fftSize) / hopSize + 1u;
         overlapBufSize             = numInputSamples - (firstFrameWindows * hopSize);
      }
   }

   return overlapBufSize;
}

template <typename T>
uint32_t Stft<T>::calcNumWindows(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples) noexcept
{
   uint32_t numWindows = 0u;

   if (hopSize != 0u) {
      uint32_t overlapBufSize = calcOverlapBufSize(fftSize, hopSize, numInputSamples);
      numWindows              = (overlapBufSize + numInputSamples - fftSize) / hopSize + 1u;
   }

   return numWindows;
}

template <typename T>
uint32_t Stft<T>::calcScratchSize(uint32_t fftSize,
                                   uint32_t hopSize,
                                   uint32_t numInputSamples,
                                   uint32_t numChannels) noexcept
{
   uint32_t scratchSize = 0u;

   if (hopSize != 0u) {
      uint32_t numWindows     = calcNumWindows(fftSize, hopSize, numInputSamples);
      uint32_t overlapBufSize = calcOverlapBufSize(fftSize, hopSize, numInputSamples);

      /* when numWindows==1 and numChannels>1, batch FFT across channels */
      uint32_t batchSize = numWindows;
      if (numWindows == 1u && numChannels > 1u) {
         batchSize = numChannels;
      }

      constexpr uint32_t alignElems     = static_cast<uint32_t>(TISP_SE_SA_ALIGN_BYTES / sizeof(T));
      uint32_t           firstHalfSize  = ((batchSize * fftSize) + alignElems - 1u) & ~(alignElems - 1u);
      uint32_t           stagingSize    = overlapBufSize + numInputSamples;
      uint32_t           secondHalfSize = (batchSize * fftSize > stagingSize) ? (batchSize * fftSize) : stagingSize;
      scratchSize                       = firstHalfSize + secondHalfSize;
   }

   return scratchSize;
}

template <typename T>
float Stft<T>::calcTotalMemKB(uint32_t fftSize,
                              uint32_t hopSize,
                              uint32_t numChannels,
                              uint32_t numInputSamples,
                              bool     onesided) noexcept
{
   float totalMemKB = 0.0f;

   if (hopSize != 0u) {
      uint32_t numWindows     = calcNumWindows(fftSize, hopSize, numInputSamples);
      uint32_t overlapBufSize = calcOverlapBufSize(fftSize, hopSize, numInputSamples);
      uint32_t outBins        = onesided ? (fftSize + 2u) : (2u * fftSize);
      uint32_t scratchElems   = calcScratchSize(fftSize, hopSize, numInputSamples, numChannels);

      uint32_t totalElems = (numInputSamples * numChannels) + (numWindows * outBins * numChannels) + fftSize +
                            (overlapBufSize * numChannels) + scratchElems;

      totalMemKB = static_cast<float>(totalElems * sizeof(T)) / 1024.0f;
   }

   return totalMemKB;
}

template <typename T> void Stft<T>::cleanInternalBuffers() noexcept { std::memset(_pState, 0, _historyBufSize); }

/***************************/
/* Explicit instantiation  */
/***************************/

template class Stft<float>;

} /* namespace AUDIOLIB */
} /* namespace TISP */
