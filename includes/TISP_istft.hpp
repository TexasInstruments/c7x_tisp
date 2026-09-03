// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include "TISP_ifftr1dBatched.hpp"

#include <cstdint>
#include <memory>

namespace TISP {
namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 *  \class Istft
 *  @brief Provides node-level APIs for the Inverse Short-Time Fourier
 *         Transform (ISTFT).
 *
 *  @details This node implements a streaming ISTFT that composes the existing
 *           TISP::FFTLIB::IFFTR1dBatched node for the inverse FFT stage. Each
 *           exec() call consumes numWindows complex spectral frames per
 *           channel and reconstructs numWindows * hopSize real output samples
 *           per channel via synthesis windowing and overlap-add (OLA).
 *
 *  This is the inverse of TISP::AUDIOLIB::Stft and mirrors torch.istft
 *  (with center=False semantics). The golden reference is AUDIOLIB_istft_cn
 *  in the test tree.
 *
 *  Algorithm per exec() (single channel):
 *  1. Batched inverse real FFT of all numWindows frames (complex -> real).
 *  2. Apply the synthesis window to every reconstructed time-domain frame.
 *  3. Overlap-add the windowed frames into the running OLA state, emitting
 *     hopSize output samples per frame and saving the unconsumed tail.
 *
 *  Multi-channel inputs are processed channel-by-channel, reusing the same
 *  scratch memory for each channel. The OLA state buffer persists across
 *  exec() calls for continuous block-by-block streaming; the constructor
 *  zero-initializes it so the first call sees silence in the tail.
 *
 *  Buffer sizes (floats):
 *
 *  | Buffer            | Size                                  | Lifetime   |
 *  |-------------------|---------------------------------------|------------|
 *  | pX (input)        | numChannels * numWindows * inBins     | per-block  |
 *  | pY (output)       | numChannels * numWindows * hopSize    | per-block  |
 *  | pWindow           | fftSize                               | persistent |
 *  | pOlaBuf           | (fftSize - hopSize) * numChannels     | persistent |
 *  | pScratch          | calcScratchSize(fftSize, numWindows)  | per-block  |
 *
 *  where inBins = (fftSize + 2) when onesided, else 2 * fftSize.
 *  pScratch is split internally: the first half holds the real inverse-FFT
 *  output frames (numWindows * fftSize, rounded up to a 64-byte boundary),
 *  the second half serves both as the IFFTR1dBatched butterfly scratch and as
 *  the windowed-frame buffer (sized for the larger of the two, the FFT scratch).
 *
 *  @pre hopSize <= fftSize.
 *  @pre numWindows >= 1.
 *  @pre pWindow must contain a valid fftSize-length synthesis window.
 *  @pre pScratch must be 64-byte aligned.
 *
 *  @tparam T  Supports float.
 */

template <typename T> class Istft final : public VirtualOp {
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pX;          /*!< Input spectrum [numChannels x numWindows x inBins], complex interleaved */
   T *_pY;          /*!< Output samples [numChannels x numWindows*hopSize], real */
   T *_pWindow;     /*!< Synthesis window [fftSize], real, caller-owned persistent */
   T *_pOlaBuf;     /*!< Overlap-add state [(fftSize-hopSize) x numChannels], caller-owned persistent */
   T *_pIFFTOut;    /*!< Real inverse-FFT output frames [numWindows x fftSize], first scratch region */
   T *_pFftScratch; /*!< IFFTR butterfly scratch + windowed frames, second scratch region */
   T *_pEnvelope;   /*!< Reciprocated steady-state window envelope [hopSize], third scratch region */
   T  _normEps;     /*!< Epsilon added to the envelope before reciprocation, guards a zero-energy hop */

   uint32_t _fftSize;          /*!< FFT length */
   uint32_t _hopSize;          /*!< Hop size in samples */
   uint32_t _numChannels;      /*!< Number of independent channels */
   uint32_t _numWindows;       /*!< Number of input spectral frames per exec() call */
   uint32_t _inBins;           /*!< Complex-interleaved floats per frame = onesided ? N+2 : 2N */
   uint32_t _overlapBufSize;   /*!< Unconsumed tail size per channel = fftSize - hopSize */
   uint32_t _numOutputSamples; /*!< Output samples per channel per exec() = numWindows * hopSize */
   bool     _onesided;         /*!< true = N/2+1 bins, false = N bins */
   size_t   _historyBufSize;

   std::unique_ptr<TISP::FFTLIB::IFFTR1dBatched<T>, NodeDeleter> _ifftNode; /*!< Internal inverse FFT node */

   void (Istft::*_execImpl)() noexcept; /*!< Exec dispatch: execMultiWindow or execBatchChannels */

   void execMultiWindow() noexcept;
   void execBatchChannels() noexcept;

   using vec = typename c7x::make_full_vector<T>::type;

   __SE_TEMPLATE_v1 _seFrames      = __gen_SE_TEMPLATE_v1(); /*!< SE0: 1D read of IFFT output frames */
   __SE_TEMPLATE_v1 _seWindow      = __gen_SE_TEMPLATE_v1(); /*!< SE1: 2D repeated read of synthesis window */
   __SA_TEMPLATE_v1 _saOutput      = __gen_SA_TEMPLATE_v1(); /*!< SA0: 1D write of windowed frames */
   uint32_t         _numWindowVecs = 0u;                     /*!< Vector iterations for the windowing loop */

   /* overlap-add scatter (step 3.2): accumulate each frame at its hopSize timeline offset. */
   __SE_TEMPLATE_v1 _seScatterFrame      = __gen_SE_TEMPLATE_v1(); /*!< SE0: 2D read of windowed frames    */
   __SE_TEMPLATE_v1 _seScatterAcc        = __gen_SE_TEMPLATE_v1(); /*!< SE1: 2D read of OLA accumulator      */
   __SA_TEMPLATE_v1 _saScatter           = __gen_SA_TEMPLATE_v1(); /*!< SA0: 2D RMW write, DIM1 = hopSize   */
   uint32_t         _numScatterVecs      = 0u; /*!< Batch path: flat iterations over numChannels*fftSize */
   uint32_t         _scatterVecsPerFrame = 0u; /*!< Multi-window path: iterations per frame = ceil(fftSize/veclen) */

   /* envelope normalization (step 3.3): scale each accumulated hop by the reciprocal envelope. */
   __SE_TEMPLATE_v1 _seEmitWork  = __gen_SE_TEMPLATE_v1(); /*!< SE0: 3D read of OLA accumulator      */
   __SE_TEMPLATE_v1 _seEmitEnv   = __gen_SE_TEMPLATE_v1(); /*!< SE1: 3D per-frame read of 1/envelope */
   __SA_TEMPLATE_v1 _saEmit      = __gen_SA_TEMPLATE_v1(); /*!< SA0: 3D write of output samples       */
   uint32_t         _numEmitVecs = 0u;                     /*!< Vector iterations = numWindows*vecs  */

   /* batch-channels path (numWindows==1, numChannels>1): cross-channel state preload and tail
    * save in single SE/SA passes, mirroring Stft::execBatchChannels. */
   __SE_TEMPLATE_v1 _seStateCopy  = __gen_SE_TEMPLATE_v1(); /*!< SE0: 2D read of OLA state (all channels)    */
   __SA_TEMPLATE_v1 _saStateCopy  = __gen_SA_TEMPLATE_v1(); /*!< SA0: 2D scatter of state into workbuf       */
   __SE_TEMPLATE_v1 _seTailRead   = __gen_SE_TEMPLATE_v1(); /*!< SE0: 2D read of workbuf tail (all channels) */
   __SA_TEMPLATE_v1 _saTailWrite  = __gen_SA_TEMPLATE_v1(); /*!< SA0: 2D write of OLA state (all channels)   */
   uint32_t         _numStateVecs = 0u;                     /*!< Vector iterations for state copy / tail save */

 public:
   /**
    * @brief Creates an ISTFT node.
    *
    * @param [in]  *pX          : Input complex spectrum
    *                             [numChannels x numWindows x inBins]. Initializes @ref _pX.
    * @param [out] *pY          : Output real samples
    *                             [numChannels x numWindows*hopSize]. Initializes @ref _pY.
    * @param [in]  *pWindow     : Synthesis window [fftSize], caller-owned. When the window
    *                             is shorter than fftSize, use the center-padded windowGen
    *                             overload to produce a zero-padded fftSize buffer.
    *                             Initializes @ref _pWindow.
    * @param [in]  *pOlaBuf     : Overlap-add state [(fftSize-hopSize) x numChannels],
    *                             caller-owned, persistent. Zero-initialized by the constructor.
    *                             Initializes @ref _pOlaBuf.
    * @param [in]  *pScratch    : Scratch buffer; size in floats from calcScratchSize().
    *                             Caller-owned, 64-byte aligned. Split into
    *                             @ref _pIFFTOut, @ref _pFftScratch, and @ref _pEnvelope.
    * @param [in]  fftSize      : FFT length. Initializes @ref _fftSize.
    * @param [in]  hopSize      : Hop size in samples. Initializes @ref _hopSize.
    * @param [in]  numChannels  : Number of channels. Initializes @ref _numChannels.
    * @param [in]  numWindows   : Number of input spectral frames per exec(). Initializes @ref _numWindows.
    * @param [in]  onesided     : Input mode. true = half spectrum (N/2+1 bins), false = full (N bins).
    *                             Initializes @ref _onesided.
    * @param [in,out] errorCtx  : Error context for reporting construction errors.
    *
    */
   Istft(T         *pX,
         T         *pY,
         T         *pWindow,
         T         *pOlaBuf,
         T         *pScratch,
         uint32_t   fftSize,
         uint32_t   hopSize,
         uint32_t   numChannels,
         uint32_t   numWindows,
         bool       onesided,
         ErrorCtxt &errorCtx);

   /**
    * @brief Creates an ISTFT node with a name and ID (delegating constructor).
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Istft(T          *pX,
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
         ErrorCtxt  &errorCtx);

   /**
    * @brief Set source and destination addresses for the operation.
    *
    * @param [in] addrs : src[0] = pX, dst[0] = pY.
    * @param [in] mode  : Address-setting mode (input, output, or both).
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute streaming ISTFT for all channels.
    *
    * Dispatches to execMultiWindow (general per-channel path) or
    * execBatchChannels (single-window multi-channel batched path).
    * The dispatch target is set once in the constructor.
    *
    */
   void exec() noexcept override { (this->*_execImpl)(); }

   /**
    * @brief Get the name of the node.
    * @retval const char* The node name.
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node.
    * @retval uint32_t The node ID.
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameter description of the node.
    * @retval const char* The parameter string.
    */
   const char *getNodeParams() const noexcept override { return _nodeInfo.getNodeParams(); }

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pX (input array)
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pY (output array)
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _pX)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pY)
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Get the input and output buffer parameters of the node
    *
    * This function returns reference to node's i/o buffer properties
    *
    * @retval const NodeBufParams& : A constant reference to the NodeBufParams representing node's i/o buffer properties
    */
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Get the overlap-add buffer size per channel (unconsumed tail).
    * @retval uint32_t The OLA buffer size in samples.
    */
   uint32_t getOlaBufSize() const noexcept;

   /**
    * @brief Get the number of output samples per channel per exec() call.
    * @retval uint32_t The output sample count.
    */
   uint32_t getNumOutputSamples() const noexcept;

   /**
    * @brief Compute the overlap-add buffer size before constructing the node.
    * @param [in] fftSize : FFT length.
    * @param [in] hopSize : Hop size in samples.
    * @retval uint32_t Overlap buffer size per channel (fftSize - hopSize).
    */
   static uint32_t calcOlaBufSize(uint32_t fftSize, uint32_t hopSize) noexcept;

   /**
    * @brief Compute the output sample count before constructing the node.
    * @param [in] numWindows : Number of input spectral frames per exec().
    * @param [in] hopSize    : Hop size in samples.
    * @retval uint32_t Output samples per channel per exec() (numWindows * hopSize).
    */
   static uint32_t calcNumOutputSamples(uint32_t numWindows, uint32_t hopSize) noexcept;

   /**
    * @brief Compute the scratch buffer size (floats) before constructing the node.
    *
    * When numWindows == 1 and numChannels > 1, the node batches the inverse FFT
    * across channels, requiring larger scratch (batchSize = numChannels instead
    * of numWindows). The default numChannels = 1 preserves backward compatibility
    * for single-channel callers.
    *
    * @param [in] fftSize     : FFT length.
    * @param [in] numWindows  : Number of input spectral frames per exec().
    * @param [in] numChannels : Number of channels (default 1).
    * @retval uint32_t Scratch buffer size in elements of type T.
    */
   static uint32_t calcScratchSize(uint32_t fftSize, uint32_t numWindows, uint32_t numChannels = 1u) noexcept;

   /**
    * @brief Compute total memory usage for all ISTFT buffers in kB.
    * @param [in] fftSize     : FFT length.
    * @param [in] hopSize     : Hop size in samples.
    * @param [in] numChannels : Number of channels.
    * @param [in] numWindows  : Number of input spectral frames per exec().
    * @param [in] onesided    : Input mode (true = N/2+1 bins, false = N bins).
    * @retval float Total memory in kB across all buffers.
    */
   static float calcTotalMemKB(uint32_t fftSize,
                               uint32_t hopSize,
                               uint32_t numChannels,
                               uint32_t numWindows,
                               bool     onesided) noexcept;

   /**
    * @brief Clean and reset all internal state and history buffers to zero
    *
    */
   void cleanInternalBuffers() noexcept override;

   /**
    * @brief Destructor.
    */
   ~Istft() = default;
};

/** @} */

} /* namespace AUDIOLIB */
} /* namespace TISP */
