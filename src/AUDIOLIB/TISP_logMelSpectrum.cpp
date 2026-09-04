// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: Apache-2.0

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_logMelSpectrum.hpp>

#include <c7x.h>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace TISP {
namespace AUDIOLIB {

namespace {

/** @brief Round an element count up to the SE/SA alignment boundary. */
inline uint32_t alignUpElems(uint32_t count)
{
   constexpr uint32_t alignElems = static_cast<uint32_t>(TISP_SE_SA_ALIGN_BYTES / sizeof(float));
   return (count + alignElems - 1u) & ~(alignElems - 1u);
}

/* Newton-Raphson refinement coefficients for the single rsqrt step in the
 * magnitude kernel: y1 = y0 * (kRsqrtNewtonThreeHalves - kRsqrtNewtonHalf * x * y0^2). */
constexpr double kRsqrtNewtonHalf        = 0.5;
constexpr double kRsqrtNewtonThreeHalves = 1.5;

} /* anonymous namespace */

template <typename T>
LogMelSpectrum<T>::LogMelSpectrum(T         *pX,
                                  T         *pY,
                                  T         *pWindow,
                                  T         *pMel,
                                  T         *pState,
                                  T         *pScratch,
                                  uint32_t   fftSize,
                                  uint32_t   hopSize,
                                  uint32_t   numChannels,
                                  uint32_t   numInputSamples,
                                  uint32_t   numMelBins,
                                  T          power,
                                  T          logOffset,
                                  ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY), _pWindow(pWindow), _pMel(pMel), _pState(pState), _pScratch(pScratch),
      _pStftScratch(nullptr), _pComplex(nullptr), _pPower(nullptr), _pMelSpec(nullptr),
      _fftSize(fftSize), _hopSize(hopSize), _numChannels(numChannels), _numInputSamples(numInputSamples),
      _numMelBins(numMelBins), _numSpecBins((fftSize / 2u) + 1u), _numWindows(0u), _overlapBufSize(0u),
      _power(power), _logOffset(logOffset)
{
   /* fill params first so a node that fails validation still reports its configuration */
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                   "fftSize=%u,hopSize=%u,numCh=%u,numInputSamples=%u,numMelBins=%u", fftSize, hopSize, numChannels,
                   numInputSamples, numMelBins);

   /* bind the spectrum kernel once here so exec() stays branch-free */
   if (power == static_cast<T>(2)) {
      _computeSpectrum = &LogMelSpectrum<T>::computeMagSquared;
   }
   else if (power == static_cast<T>(1)) {
      _computeSpectrum = &LogMelSpectrum<T>::computeMagnitude;
   }
   else {
      _computeSpectrum = &LogMelSpectrum<T>::computePowerGeneral;
   }

   /* input validation */
   if (pX == nullptr) {
      errorCtx.setNullPointerError("LogMelSpectrum constructor", "input pointer pX is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("LogMelSpectrum constructor", "output pointer pY is null");
   }
   if (pWindow == nullptr) {
      errorCtx.setNullPointerError("LogMelSpectrum constructor", "window pointer pWindow is null");
   }
   if (pMel == nullptr) {
      errorCtx.setNullPointerError("LogMelSpectrum constructor", "mel matrix pointer pMel is null");
   }
   if (pScratch == nullptr) {
      errorCtx.setNullPointerError("LogMelSpectrum constructor", "scratch pointer pScratch is null");
   }
   if (fftSize == 0u) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "fftSize must be greater than zero");
   }
   else if ((fftSize & (fftSize - 1u)) != 0u) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "fftSize must be a power of two");
   }
   if (hopSize == 0u) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "hopSize must be greater than zero");
   }
   else if (hopSize > fftSize) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "hopSize must not exceed fftSize");
   }
   if (numChannels == 0u) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "numChannels must be greater than zero");
   }
   if (numMelBins == 0u) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "numMelBins must be greater than zero");
   }
   if (power <= static_cast<T>(0)) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "power must be greater than zero");
   }
   if (numInputSamples == 0u) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "numInputSamples must be greater than zero");
   }
   else if ((hopSize != 0u) && (numInputSamples < hopSize)) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "numInputSamples must be >= hopSize");
   }
   if ((hopSize != 0u) && ((numInputSamples % hopSize) != 0u)) {
      errorCtx.setInvalidParameterError("LogMelSpectrum constructor", "numInputSamples must be a multiple of hopSize");
   }

   /* derived geometry — only after the scalar params are validated */
   if (errorCtx.isSuccess()) {
      _numWindows     = calcNumWindows(fftSize, hopSize, numInputSamples);
      _overlapBufSize = calcOverlapBufSize(fftSize, hopSize, numInputSamples);

      /* input waveform [numChannels x numInputSamples], output log-mel [numChannels x numWindows x numMelBins] */
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = _numInputSamples;
      _nodeInfo.bufParams.heightIn[0]       = _numChannels;
      _nodeInfo.bufParams.inPitchBytes[0]   = _numInputSamples * sizeof(T);

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = _numWindows * _numMelBins;
      _nodeInfo.bufParams.heightOut[0]       = _numChannels;
      _nodeInfo.bufParams.outPitchBytes[0]   = _numWindows * _numMelBins * sizeof(T);

      if ((pState == nullptr) && (_overlapBufSize != 0u)) {
         errorCtx.setNullPointerError("LogMelSpectrum constructor", "state pointer pState is null");
      }
   }

   if (errorCtx.isSuccess()) {
      /* carve the caller scratch into aligned sub-regions.
       * layout: [stftScratch | complex | power | melSpec].
       * complex/power/mel are batched over all channels (Stft emits all channels).
       * All |.|^power paths compute the spectrogram (MatMul's A input) in place
       * in _pPower. */
      const uint32_t totalFrames  = _numChannels * _numWindows;
      const bool     generalPower = (_power != static_cast<T>(1)) && (_power != static_cast<T>(2));

      const uint32_t stftElems    = Stft<T>::calcScratchSize(_fftSize, _hopSize, _numInputSamples, _numChannels);
      const uint32_t complexElems = totalFrames * (_fftSize + 2u);
      const uint32_t powerElems   = totalFrames * _numSpecBins;

      const uint32_t offStft    = 0u;
      const uint32_t offComplex = offStft + alignUpElems(stftElems);
      const uint32_t offPower   = offComplex + alignUpElems(complexElems);
      const uint32_t offMelSpec = offPower + alignUpElems(powerElems);

      _pStftScratch = &_pScratch[offStft];
      _pComplex     = &_pScratch[offComplex];
      _pPower       = &_pScratch[offPower];
      _pMelSpec     = &_pScratch[offMelSpec];

      /* internal windowing + batched RFFT: pX -> _pComplex (onesided half spectrum).
       * Stft owns the overlap-state framing (and zero-inits pState). */
      _stftNode = TISP::make_unique_node<Stft<T>>(_pX, _pComplex, _pWindow, _pState, _pStftScratch, _fftSize, _hopSize,
                                                  _numChannels, _numInputSamples, true /* onesided */, errorCtx);
      if (_stftNode == nullptr) {
         errorCtx.setMemoryError("LogMelSpectrum constructor", "failed to allocate Stft node from L2 arena");
      }

      /* internal mel matmul: [totalFrames x numSpecBins] * [numSpecBins x numMelBins] -> [totalFrames x numMelBins]. */
      if (errorCtx.isSuccess()) {
         _matMulNode = TISP::make_unique_node<TISP::DSPLIB::MatMul<T>>(
             _pPower, _pMel, _pMelSpec, static_cast<size_t>(_numSpecBins), static_cast<size_t>(totalFrames),
             static_cast<size_t>(_numMelBins), static_cast<size_t>(_numSpecBins),
             static_cast<size_t>(_numSpecBins) * sizeof(T), static_cast<size_t>(_numMelBins) * sizeof(T),
             static_cast<size_t>(_numMelBins) * sizeof(T), errorCtx);
         if (_matMulNode == nullptr) {
            errorCtx.setMemoryError("LogMelSpectrum constructor", "failed to allocate MatMul node from L2 arena");
         }
      }

      /* internal offset-add: broadcast +logOffset in-place on the mel spectrogram. */
      if (errorCtx.isSuccess()) {
         _addOffsetNode = TISP::make_unique_node<TISP::DSPLIB::AddConstant<T>>(
             &_logOffset, _pMelSpec, _pMelSpec, static_cast<size_t>(totalFrames) * _numMelBins, errorCtx);
         if (_addOffsetNode == nullptr) {
            errorCtx.setMemoryError("LogMelSpectrum constructor", "failed to allocate AddConstant node from L2 arena");
         }
      }

      /* internal natural log: _pMelSpec -> pY (all channels, batched). */
      if (errorCtx.isSuccess()) {
         _logNode = TISP::make_unique_node<TISP::MATHLIB::Log<T>>(
             static_cast<size_t>(totalFrames) * _numMelBins, _pMelSpec, _pY, errorCtx);
         if (_logNode == nullptr) {
            errorCtx.setMemoryError("LogMelSpectrum constructor", "failed to allocate Log node from L2 arena");
         }
      }

      if (errorCtx.isSuccess() && generalPower) {
         /* general |.|^p = exp((p/2) * ln(mag2)); computed in place in _pPower. */
         const size_t specElems = static_cast<size_t>(totalFrames) * _numSpecBins;
         _powLogNode            = TISP::make_unique_node<TISP::MATHLIB::Log<T>>(specElems, _pPower, _pPower, errorCtx);
         _powExpNode            = TISP::make_unique_node<TISP::MATHLIB::Exp<T>>(specElems, _pPower, _pPower, errorCtx);
         if ((_powLogNode == nullptr) || (_powExpNode == nullptr)) {
            errorCtx.setMemoryError("LogMelSpectrum constructor", "failed to allocate power ln/exp nodes");
         }
      }

      /* mag2 SE/SA templates. */
      {
         using fvec = typename c7x::make_full_vector<T>::type;

         const uint32_t specElems = totalFrames * _numSpecBins; /* complex = output reals */

         _seMagDec.ELETYPE = c7x::se_eletype<fvec>::value;
         _seMagDec.VECLEN  = c7x::se_veclen<fvec>::value;
         _seMagDec.DIMFMT  = __SE_DIMFMT_1D;
         _seMagDec.DECIM   = __SE_DECIM_2;
         _seMagDec.ICNT0   = 2u * specElems;

         _saMagFull.VECLEN = c7x::sa_veclen<fvec>::value;
         _saMagFull.DIMFMT = __SA_DIMFMT_1D;
         _saMagFull.ICNT0  = specElems;

         /* full-width real in-place scale for the general ln -> scale -> exp path */
         _seScale.ELETYPE = c7x::se_eletype<fvec>::value;
         _seScale.VECLEN  = c7x::se_veclen<fvec>::value;
         _seScale.DIMFMT  = __SE_DIMFMT_1D;
         _seScale.ICNT0   = specElems;

         _saScale.VECLEN = c7x::sa_veclen<fvec>::value;
         _saScale.DIMFMT = __SA_DIMFMT_1D;
         _saScale.ICNT0  = specElems;
      }
   }
}

template <typename T>
LogMelSpectrum<T>::LogMelSpectrum(T          *pX,
                                  T          *pY,
                                  T          *pWindow,
                                  T          *pMel,
                                  T          *pState,
                                  T          *pScratch,
                                  uint32_t    fftSize,
                                  uint32_t    hopSize,
                                  uint32_t    numChannels,
                                  uint32_t    numInputSamples,
                                  uint32_t    numMelBins,
                                  T           power,
                                  T           logOffset,
                                  const char *nodeName,
                                  uint32_t    nodeId,
                                  ErrorCtxt  &errorCtx)
    : LogMelSpectrum(pX, pY, pWindow, pMel, pState, pScratch, fftSize, hopSize, numChannels, numInputSamples,
                     numMelBins, power, logOffset, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

/* =========================================================================
 * exec stages
 * ========================================================================= */

/* power == 2: |z|^2 = re^2 + im^2.*/
template <typename T> void LogMelSpectrum<T>::computeMagSquared() noexcept
{
   using fvec = typename c7x::make_full_vector<T>::type;

   T *restrict    pOut    = _pPower;
   const uint32_t n       = _numChannels * _numWindows * _numSpecBins;
   const int32_t  numVecs = static_cast<int32_t>((n + c7x::element_count_of<fvec>::value - 1u) /
                                                 c7x::element_count_of<fvec>::value);

   __SE0_OPEN(_pComplex, _seMagDec);
   __SE1_OPEN(&_pComplex[1], _seMagDec);
   __SA0_OPEN(_saMagFull);
   //8 + trip_cnt * 1
   for (int32_t v = 0; v < numVecs; v++) {
      fvec    re   = c7x::strm_eng<0, fvec>::get_adv(); /* reals */
      fvec    im   = c7x::strm_eng<1, fvec>::get_adv(); /* imags */
      fvec    mag2 = (re * re) + (im * im);
      __vpred pred = c7x::strm_agen<0, fvec>::get_vpred();
      fvec   *addr = c7x::strm_agen<0, fvec>::get_adv(pOut);
      __vstore_pred(pred, addr, mag2);
   }
   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();
}

/* power == 1: |z| = sqrt(mag2). Fuses mag2 and sqrt in a single SE/SA pass; sqrt
 * via rsqrt + one Newton step, with a zero guard (mag2 <= 0 -> 0) for silence
*/
template <typename T> void LogMelSpectrum<T>::computeMagnitude() noexcept
{
   using fvec = typename c7x::make_full_vector<T>::type;

   T *restrict    pOut    = _pPower;
   const uint32_t n       = _numChannels * _numWindows * _numSpecBins;
   const int32_t  numVecs = static_cast<int32_t>((n + c7x::element_count_of<fvec>::value - 1u) /
                                                 c7x::element_count_of<fvec>::value);
   const fvec     half    = (fvec) static_cast<T>(kRsqrtNewtonHalf);
   const fvec     oneP5   = (fvec) static_cast<T>(kRsqrtNewtonThreeHalves);
   const fvec     zero    = (fvec) static_cast<T>(0);

   __SE0_OPEN(_pComplex, _seMagDec);
   __SE1_OPEN(&_pComplex[1], _seMagDec);
   __SA0_OPEN(_saMagFull);
   for (int32_t v = 0; v < numVecs; v++) {
      fvec re   = c7x::strm_eng<0, fvec>::get_adv(); /* reals */
      fvec im   = c7x::strm_eng<1, fvec>::get_adv(); /* imags */
      fvec mag2 = (re * re) + (im * im);

      fvec p0 = __recip_sqrt(mag2);       /* ~1/sqrt(mag2)             */
      fvec d0 = p0 * mag2;
      fvec p1 = oneP5 - (d0 * p0 * half); /* Newton refinement         */
      fvec y  = mag2 * p0 * p1;           /* sqrt(mag2) = mag2 * rsqrt */

      __vpred le0 = __cmp_le_pred(mag2, zero);
      y           = __select(le0, zero, y); /* mag2 <= 0 -> 0 (silence) */

      __vpred pred = c7x::strm_agen<0, fvec>::get_vpred();
      fvec   *addr = c7x::strm_agen<0, fvec>::get_adv(pOut);
      __vstore_pred(pred, addr, y);
   }
   __SE0_CLOSE();
   __SE1_CLOSE();
   __SA0_CLOSE();
}

/*
 * general power p (p != 1, 2): |z|^p = (re^2+im^2)^(p/2) = exp((p/2)*ln(mag2)),
 * computed in place in _pPower: mag2 -> ln (node) -> scale by p/2 (inline SE
 * loop) -> exp (node).
 */
template <typename T> void LogMelSpectrum<T>::computePowerGeneral() noexcept
{
   using fvec = typename c7x::make_full_vector<T>::type;

   T *restrict    pOut    = _pPower;
   const uint32_t n       = _numChannels * _numWindows * _numSpecBins;
   const fvec     halfV   = (fvec) (_power * static_cast<T>(0.5));
   const int32_t  numVecs = static_cast<int32_t>((n + c7x::element_count_of<fvec>::value - 1u) /
                                                 c7x::element_count_of<fvec>::value);

   computeMagSquared(); /* _pPower = mag2     */
   _powLogNode->exec(); /* _pPower = ln(mag2) */

   __SE0_OPEN(_pPower, _seScale);
   __SA0_OPEN(_saScale);
   for (int32_t v = 0; v < numVecs; v++) {
      fvec    x    = c7x::strm_eng<0, fvec>::get_adv();
      __vpred pred = c7x::strm_agen<0, fvec>::get_vpred();
      fvec   *addr = c7x::strm_agen<0, fvec>::get_adv(pOut);
      __vstore_pred(pred, addr, x * halfV); /* scale by p/2 */
   }
   __SE0_CLOSE();
   __SA0_CLOSE();

   _powExpNode->exec(); /* _pPower = exp((p/2)*ln(mag2)) = mag2^(p/2) */
}

template <typename T> void LogMelSpectrum<T>::exec() noexcept
{
   _stftNode->exec();           /* pX -> _pComplex (windowed, batched RFFT, all channels) */
   (this->*_computeSpectrum)(); /* _pComplex -> _pPower (|.|^power), kernel bound at ctor  */
   _matMulNode->exec();         /* _pPower * _pMel -> _pMelSpec                            */
   _addOffsetNode->exec();      /* _pMelSpec += _logOffset (broadcast, in place)           */
   _logNode->exec();            /* log(_pMelSpec) -> _pY                                   */
}

/* =========================================================================
 * VirtualOp accessors / setAddr
 * ========================================================================= */

template <typename T> void *LogMelSpectrum<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *LogMelSpectrum<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t LogMelSpectrum<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t LogMelSpectrum<T>::getNumOutputs() const noexcept { return 1; }

/* Update this node's I/O and immediately propagate it to the composed endpoints:
 * the internal Stft consumes the input (pX) and the internal Log produces the
 * output (pY). Interior scratch pointers are fixed, so nothing else needs
 * rebinding. Rebinding here (rather than deferring to exec()) keeps the node and
 * its sub-nodes consistent as soon as setAddr() returns. */
template <typename T> void LogMelSpectrum<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if ((mode == kAddrModeIn) || (mode == kAddrModeInOut)) {
      _pX = static_cast<T *>(addr.src[0]);
      SetAddr_t stftAddr{};
      stftAddr.src[0] = _pX;
      _stftNode->setAddr(stftAddr, kAddrModeIn);
   }
   if ((mode == kAddrModeOut) || (mode == kAddrModeInOut)) {
      _pY = static_cast<T *>(addr.dst[0]);
      SetAddr_t logAddr{};
      logAddr.dst[0] = _pY;
      _logNode->setAddr(logAddr, kAddrModeOut);
   }
}

template <typename T> uint32_t LogMelSpectrum<T>::getNumWindows() const noexcept { return _numWindows; }

template <typename T> uint32_t LogMelSpectrum<T>::getNumSpectrogramBins() const noexcept { return _numSpecBins; }

template <typename T> uint32_t LogMelSpectrum<T>::getOverlapBufSize() const noexcept { return _overlapBufSize; }

/* =========================================================================
 * static planners
 * ========================================================================= */

template <typename T>
uint32_t
LogMelSpectrum<T>::calcOverlapBufSize(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples) noexcept
{
   uint32_t overlapBufSize;
   if (numInputSamples < fftSize) {
      overlapBufSize = fftSize - hopSize;
   }
   else {
      uint32_t firstFrameWindows = (numInputSamples - fftSize) / hopSize + 1u;
      overlapBufSize             = numInputSamples - (firstFrameWindows * hopSize);
   }
   return overlapBufSize;
}

template <typename T>
uint32_t LogMelSpectrum<T>::calcNumWindows(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples) noexcept
{
   uint32_t overlapBufSize = calcOverlapBufSize(fftSize, hopSize, numInputSamples);
   return (overlapBufSize + numInputSamples - fftSize) / hopSize + 1u;
}

template <typename T> uint32_t LogMelSpectrum<T>::calcNumSpectrogramBins(uint32_t fftSize) noexcept
{
   return (fftSize / 2u) + 1u;
}

template <typename T>
uint32_t LogMelSpectrum<T>::calcScratchSize(uint32_t fftSize,
                                            uint32_t hopSize,
                                            uint32_t numInputSamples,
                                            uint32_t numMelBins,
                                            uint32_t numChannels) noexcept
{
   const uint32_t numWindows   = calcNumWindows(fftSize, hopSize, numInputSamples);
   const uint32_t numSpecBins  = calcNumSpectrogramBins(fftSize);
   const uint32_t totalFrames  = numChannels * numWindows;

   /* layout: [stftScratch | complex | power | melSpec]. All |.|^power paths compute
    * the spectrogram in place in the power region, so the size is power-independent. */
   const uint32_t stftElems    = Stft<T>::calcScratchSize(fftSize, hopSize, numInputSamples, numChannels);
   const uint32_t complexElems = totalFrames * (fftSize + 2u);
   const uint32_t powerElems   = totalFrames * numSpecBins;
   const uint32_t melElems     = totalFrames * numMelBins;

   return alignUpElems(stftElems) + alignUpElems(complexElems) + alignUpElems(powerElems) + alignUpElems(melElems);
}

template <typename T>
float LogMelSpectrum<T>::calcTotalMemKB(uint32_t fftSize,
                                        uint32_t hopSize,
                                        uint32_t numChannels,
                                        uint32_t numInputSamples,
                                        uint32_t numMelBins) noexcept
{
   const uint32_t numWindows     = calcNumWindows(fftSize, hopSize, numInputSamples);
   const uint32_t overlapBufSize = calcOverlapBufSize(fftSize, hopSize, numInputSamples);
   const uint32_t numSpecBins    = calcNumSpectrogramBins(fftSize);
   const uint32_t scratchElems   = calcScratchSize(fftSize, hopSize, numInputSamples, numMelBins, numChannels);

   const uint32_t totalElems = (numInputSamples * numChannels) + (numWindows * numMelBins * numChannels) + fftSize +
                               (numSpecBins * numMelBins) + (overlapBufSize * numChannels) + scratchElems;

   return static_cast<float>(totalElems * sizeof(T)) / 1024.0f;
}

/***************************/
/* Explicit instantiation  */
/***************************/

template class LogMelSpectrum<float>;

} /* namespace AUDIOLIB */
} /* namespace TISP */

/* ======================================================================== */
/*  End of file:  TISP_logMelSpectrum.cpp                                   */
/* ======================================================================== */
