// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_addConstant.hpp"
#include "TISP_common.hpp"
#include "TISP_exp.hpp"
#include "TISP_log.hpp"
#include "TISP_matMul.hpp"
#include "TISP_stft.hpp"

#include <cstdint>
#include <memory>

namespace TISP {
namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 *  \class LogMelSpectrum
 *  @brief Node-level API for the log-mel spectrogram feature extractor.
 *
 *  @details A log-mel spectrogram is the standard input feature for audio
 *           classification and tagging front-ends (e.g. VGGish, YAMNet). It is
 *           a compact time x mel-band "image" of a sound: a short-time Fourier
 *           transform turns the waveform into overlapping frames of frequency
 *           bins, those bins are grouped onto the perceptual mel scale, and the
 *           result is log-compressed so that quiet and loud sounds share a
 *           similar dynamic range. The output shows how energy is distributed
 *           across pitch over time.
 *
 *  @image html logMelSpectrum_concept.svg "What a log-mel spectrogram is: sound in, a time x pitch energy image out" width=900px
 *
 *  Internally the node runs a fixed pipeline: STFT -> |.|^power spectrogram -> mel
 *  filterbank matmul -> +logOffset -> natural log. The |.|^power stage is computed
 *  in-node; the other stages delegate to internal nodes.
 *
 *  @image html logMelSpectrum_pipeline.svg "Processing pipeline: waveform in, log-mel image out" width=900px
 *
 *  Streaming node: `pState` carries the overlap tail across exec() calls, so a
 *  long signal is processed block-by-block. Output layout is
 *  `[numChannels x numWindows x numMelBins]`, frame-major within each channel.
 *
 *  For the on-device optimization write-up (SE data layout, the three
 *  |.|^power paths, and per-path cost) see
 *  docs/logMelSpectrum/LogMelSpectrum_UserGuide.md.
 *
 *  **Buffers:**
 *
 *  | Buffer    | Size (floats)                          | Lifetime   |
 *  |-----------|----------------------------------------|------------|
 *  | pX        | numInputSamples * numChannels          | per-block  |
 *  | pY        | numWindows * numMelBins * numChannels  | per-block  |
 *  | pWindow   | fftSize (from TISP::windowGen)         | persistent |
 *  | pMel      | (fftSize/2+1) * numMelBins (from TISP::melFbankGen) | persistent |
 *  | pState    | overlapBufSize * numChannels           | persistent |
 *  | pScratch  | calcScratchSize(...)                   | per-block  |
 *
 *  @pre fftSize must be a power of two.
 *  @pre hopSize in [1, fftSize].
 *  @pre numInputSamples a positive multiple of hopSize and >= hopSize.
 *  @pre numMelBins > 0, power > 0.
 *  @pre pWindow holds an fftSize-length analysis window; pMel holds an
 *       [(fftSize/2+1) x numMelBins] bin-major mel matrix.
 *
 *  @tparam T  Supports float.
 */
template <typename T> class LogMelSpectrum final : public VirtualOp {
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pX;      /*!< Input waveform [numChannels x numInputSamples], real */
   T *_pY;      /*!< Output log-mel [numChannels x numWindows x numMelBins], real */
   T *_pWindow; /*!< Analysis window [fftSize], caller-owned */
   T *_pMel;    /*!< Mel filterbank [(fftSize/2+1) x numMelBins] bin-major, caller-owned */
   T *_pState;  /*!< Overlap state [overlapBufSize x numChannels], caller-owned */
   T *_pScratch; /*!< Scratch base, calcScratchSize() floats, caller-owned, 64-byte aligned */

   /* scratch sub-regions (carved from _pScratch in the constructor) */
   T *_pStftScratch; /*!< Scratch region handed to the internal Stft node */
   T *_pComplex;     /*!< Stft output, half spectrum [numChannels x numWindows x (fftSize+2)] */
   T *_pPower;       /*!< Power/magnitude spectrogram [numChannels*numWindows x numSpecBins] (MatMul A) */
   T *_pMelSpec;     /*!< Mel spectrogram [numChannels*numWindows x numMelBins] (MatMul Y, log input) */

   uint32_t _fftSize;         /*!< FFT length (power of two) */
   uint32_t _hopSize;         /*!< Hop size in samples */
   uint32_t _numChannels;     /*!< Number of channels */
   uint32_t _numInputSamples; /*!< Input samples per channel per exec() (F) */
   uint32_t _numMelBins;      /*!< Number of mel bands */
   uint32_t _numSpecBins;     /*!< One-sided FFT bins = fftSize/2 + 1 */
   uint32_t _numWindows;      /*!< Output frames per exec() (per channel) */
   uint32_t _overlapBufSize;  /*!< Streaming unconsumed tail */
   T        _power;           /*!< Magnitude exponent (1.0 or 2.0 typical) */
   T        _logOffset;       /*!< Offset added before log */

   std::unique_ptr<TISP::AUDIOLIB::Stft<T>, NodeDeleter>      _stftNode;      /*!< Internal windowing + batched RFFT */
   std::unique_ptr<TISP::DSPLIB::MatMul<T>, NodeDeleter>      _matMulNode;    /*!< Internal mel matmul (MMA) */
   std::unique_ptr<TISP::DSPLIB::AddConstant<T>, NodeDeleter> _addOffsetNode; /*!< Internal +logOffset (broadcast) */
   std::unique_ptr<TISP::MATHLIB::Log<T>, NodeDeleter>        _logNode;       /*!< Internal natural log */
   std::unique_ptr<TISP::MATHLIB::Log<T>, NodeDeleter>        _powLogNode;    /*!< ln() for the general |.|^power path (power != 1,2) */
   std::unique_ptr<TISP::MATHLIB::Exp<T>, NodeDeleter>        _powExpNode;    /*!< exp() for the general |.|^power path (power != 1,2) */

   __SE_TEMPLATE_v1 _seMagDec  = __gen_SE_TEMPLATE_v1(); /*!< SE0/SE1: 2:1-decimated re/im split for mag2 (C7504/C7524) */
   __SA_TEMPLATE_v1 _saMagFull = __gen_SA_TEMPLATE_v1(); /*!< SA0: full-width real power write (decimation path) */
   __SE_TEMPLATE_v1 _seScale   = __gen_SE_TEMPLATE_v1(); /*!< SE0: real read for the p/2 scale (general |.|^power path) */
   __SA_TEMPLATE_v1 _saScale   = __gen_SA_TEMPLATE_v1(); /*!< SA0: real write for the p/2 scale (general |.|^power path) */

   /** @brief Spectrum kernel bound once at construction from `power`, so exec()
    *  dispatches branch-free. One of computeMagSquared/Magnitude/PowerGeneral. */
   void (LogMelSpectrum<T>::*_computeSpectrum)() noexcept;

   /** @brief power==2: _pPower[i] = re^2+im^2, split with two 2:1-decimating SEs (one vectorized pass). */
   void computeMagSquared() noexcept;
   /** @brief power==1: _pPower[i] = sqrt(re^2+im^2), fusing mag2 and an rsqrt+Newton
    *  sqrt into a single SE/SA pass. */
   void computeMagnitude() noexcept;
   /** @brief general power p: _pPower[i] = (re^2+im^2)^(p/2) = exp((p/2)*ln(mag2)),
    *  computed in place via the ln node, an inline p/2 scale pass, and the exp node. */
   void computePowerGeneral() noexcept;

 public:
   /**
    * @brief Creates a LogMelSpectrum node.
    *
    * @param [in]  *pX             : Input waveform [numChannels x numInputSamples]. Initializes @ref _pX.
    * @param [out] *pY             : Output log-mel [numChannels x numWindows x numMelBins]. Initializes @ref _pY.
    * @param [in]  *pWindow        : Analysis window [fftSize] (TISP::windowGen). Initializes @ref _pWindow.
    * @param [in]  *pMel           : Mel filterbank [(fftSize/2+1) x numMelBins] bin-major
    *                                (TISP::melFbankGen). Initializes @ref _pMel.
    * @param [in]  *pState         : Overlap state [overlapBufSize x numChannels], zero-initialized by the
    *                                node; may be null only when overlapBufSize is 0. Initializes @ref _pState.
    * @param [in]  *pScratch       : Scratch buffer, calcScratchSize() floats, 64-byte aligned.
    *                                Initializes @ref _pScratch and the sub-region pointers.
    * @param [in]  fftSize         : FFT length (power of two). Initializes @ref _fftSize.
    * @param [in]  hopSize         : Hop size. Initializes @ref _hopSize.
    * @param [in]  numChannels     : Number of channels. Initializes @ref _numChannels.
    * @param [in]  numInputSamples : Input samples per channel per exec(). Initializes @ref _numInputSamples.
    * @param [in]  numMelBins      : Number of mel bands. Initializes @ref _numMelBins.
    * @param [in]  power           : Magnitude exponent (1.0=magnitude, 2.0=power). Initializes @ref _power.
    * @param [in]  logOffset       : Offset added before the log. Initializes @ref _logOffset.
    * @param [in]  errorCtx        : Error context for reporting constructor errors.
    * 
    */
   LogMelSpectrum(T         *pX,
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
                  ErrorCtxt &errorCtx);

   /**
    * @brief Creates a LogMelSpectrum node with a name and ID (delegates to the primary constructor).
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   LogMelSpectrum(T          *pX,
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
                  ErrorCtxt  &errorCtx);

   /**
    * @brief Set source/destination addresses (SuperNode DMA rebinding). src[0]=pX, dst[0]=pY.
    * @param [in] addrs : source and destination addresses.
    * @param [in] mode  : input, output, or both.
    * @note Also rebinds the internal composed Stft (input) and Log (output) nodes
    *       immediately, so the node and its sub-nodes are consistent as soon as
    *       setAddr() returns (no exec() needed to propagate the change).
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute the log-mel spectrogram over all channels.
    */
   void exec() noexcept override;

   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }
   uint32_t    getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }
   const char *getNodeParams() const noexcept override { return _nodeInfo.getNodeParams(); }
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Get an input buffer address (index 0 = pX).
    * @param[in] index : input index (0).
    * @retval void* pointer, or nullptr if index invalid.
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get an output buffer address (index 0 = pY).
    * @param[in] index : output index (0).
    * @retval void* pointer, or nullptr if index invalid.
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /** @brief Number of input buffers (1). */
   size_t getNumInputs() const noexcept override;

   /** @brief Number of output buffers (1). */
   size_t getNumOutputs() const noexcept override;

   /** @brief Output frames per exec() call. */
   uint32_t getNumWindows() const noexcept;

   /** @brief One-sided FFT bin count feeding the mel matrix. */
   uint32_t getNumSpectrogramBins() const noexcept;

   /** @brief Overlap buffer size. */
   uint32_t getOverlapBufSize() const noexcept;

   /**
    * @brief Output frames per exec() before constructing the node.
    * @param [in] fftSize,hopSize,numInputSamples : geometry.
    * @retval uint32_t frame count.
    */
   static uint32_t calcNumWindows(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples) noexcept;

   /**
    * @brief Overlap buffer size before constructing the node.
    * @param [in] fftSize,hopSize,numInputSamples : geometry.
    * @retval uint32_t overlap size in samples (per channel).
    */
   static uint32_t calcOverlapBufSize(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples) noexcept;

   /**
    * @brief One-sided FFT bin count (fftSize/2 + 1).
    * @param [in] fftSize : FFT length.
    * @retval uint32_t bin count.
    */
   static uint32_t calcNumSpectrogramBins(uint32_t fftSize) noexcept;

   /**
    * @brief Scratch buffer size (elements of T) before constructing the node.
    * @param [in] fftSize,hopSize,numInputSamples,numMelBins,numChannels : geometry.
    * @retval uint32_t scratch size in elements of T.
    */
   static uint32_t calcScratchSize(uint32_t fftSize,
                                   uint32_t hopSize,
                                   uint32_t numInputSamples,
                                   uint32_t numMelBins,
                                   uint32_t numChannels) noexcept;

   /**
    * @brief Total memory (kB) across all LogMelSpectrum buffers.
    * @param [in] fftSize,hopSize,numChannels,numInputSamples,numMelBins : geometry.
    * @retval float total kB.
    */
   static float calcTotalMemKB(uint32_t fftSize,
                               uint32_t hopSize,
                               uint32_t numChannels,
                               uint32_t numInputSamples,
                               uint32_t numMelBins) noexcept;

   /** @brief Destructor. @throw None */
   ~LogMelSpectrum() = default;
};

/** @} */

} /* namespace AUDIOLIB */
} /* namespace TISP */

/* ======================================================================== */
/*  End of file:  TISP_logMelSpectrum.hpp                                   */
/* ======================================================================== */
