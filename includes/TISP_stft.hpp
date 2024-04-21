// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_blockCopy.hpp"
#include "TISP_common.hpp"
#include "TISP_rfft1dBatched.hpp"

#include <cstdint>
#include <memory>

namespace TISP {
namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 *  \class Stft
 *  @brief Provides node-level APIs for Short-Time Fourier Transform (STFT).
 *
 *  @details This node implements a streaming STFT that composes the existing
 *           TISP::FFTLIB::RFFT1dBatched node for the FFT stage. Each exec()
 *           call processes numInputSamples (F) input samples per channel,
 *           producing numWindows output STFT frames.
 *
 *  The kernel uses the **unconsumed-tail overlap model**: the overlap state
 *  buffer holds the tail of samples from the previous block that were not
 *  consumed by any hop. This is correct for all fftSize / hopSize
 *  combinations, including cases where fftSize is not a multiple of hopSize.
 *
 *  **Derived constants (computed in constructor):**
 *  When numInputSamples >= fftSize:
 *  ```
 *  firstFrameWindows = (numInputSamples - fftSize) / hopSize + 1
 *  overlapBufSize    = numInputSamples - firstFrameWindows * hopSize
 *  numWindows        = (overlapBufSize + numInputSamples - fftSize) / hopSize + 1
 *  initPadWindows    = numWindows - firstFrameWindows
 *  ```
 *  When numInputSamples < fftSize (real-time, sub-window input):
 *  ```
 *  overlapBufSize = fftSize - hopSize
 *  numWindows     = numInputSamples / hopSize
 *  ```
 *
 *  **Algorithm per exec() (single channel):**
 *  1. Concatenate pState (overlapBufSize) + pX (numInputSamples) in staging.
 *  2. Window all numWindows frames from the concatenated buffer.
 *  3. Save the unconsumed tail to pState.
 *  4. Execute batched real FFT via RFFT1dBatched.
 *
 *  **Initial zero-padded frames:**
 *  On the first exec() call (zero-initialized state), the first
 *  `initPadWindows` output frames contain contributions from the zero-padded
 *  overlap region. These frames can be discarded if only steady-state output
 *  is desired. `initPadWindows = numWindows - firstFrameWindows`. On
 *  subsequent exec() calls, all `numWindows` frames contain valid data
 *  because the state holds real samples from the previous block.
 *  For typical configurations (N=512, H=128, F=16000): 3 out of 125 frames
 *  are zero-padded on the first call.
 *
 *  This is analogous to `torch.stft` with `center=False`: both avoid
 *  explicit padding at the start, so the first frames see zeros rather than
 *  real prior context. The key difference is that `torch.stft` is stateless
 *  and processes the entire signal at once, while this implementation
 *  maintains state across exec() calls for continuous block-by-block
 *  streaming.
 *
 *  Multi-channel inputs are processed channel-by-channel, reusing the same
 *  scratch memory for each channel.
 *
 *  **Buffer sizes:**
 *
 *  | Buffer           | Size (floats)                      | Lifetime   |
 *  |------------------|------------------------------------|------------|
 *  | pX (input)       | numInputSamples * numChannels      | per-block  |
 *  | pY (output)      | numWindows * outBins * numChannels | per-block  |
 *  | pWindow          | fftSize                            | persistent |
 *  | pState           | overlapBufSize * numChannels       | persistent |
 *  | pScratch         | align64(W * N) + max(W * N, S + F) | per-block  |
 *
 *  Where W is numWindows, N is fftSize, S is overlapBufSize, F is numInputSamples,
 *  outBins = (fftSize + 2) when onesided is true, or 2 * fftSize when false,
 *  and align64(x) = (x + 15) & ~15 rounds up to the next 64-byte boundary.
 *  pScratch is split internally: the first half (W * N)
 *  holds windowed frames, the second half is used for both concat staging
 *  (S + F) and FFT scratch (W * N) at different times, so its size must
 *  be the max of those two.
 *
 *  **Parameter dependencies (undefined behavior if violated):**
 *
 *  The derived constants above use unsigned integer division. If the
 *  following preconditions are not met, the constructor computes
 *  incorrect buffer geometry and subsequent exec() calls will read/write
 *  out of bounds or produce silently wrong output.
 *
 *  @pre numInputSamples >= hopSize.
 *      Each exec() call must produce at least one output STFT frame.
 *  @pre hopSize <= fftSize.
 *      Standard STFT constraint; the overlap formula (fftSize - hopSize)
 *      requires this to produce a non-negative overlap buffer size.
 *  @pre numInputSamples must be a multiple of hopSize.
 *      The unconsumed-tail overlap model requires that the number of
 *      samples consumed per block (numWindows * hopSize) equals
 *      numInputSamples. When numInputSamples % hopSize != 0 the saved
 *      tail size does not match the actual unconsumed region, leading
 *      to either data loss or stale-data reads across blocks.
 *  @pre pWindow must contain a valid fftSize-length analysis window.
 *      When the analysis window is shorter than fftSize (windowLength
 *      < fftSize), the caller must use the center-padded windowGen
 *      overload to produce the buffer. windowLength must be <= fftSize;
 *      violating this produces an incorrectly padded window and the
 *      windowing step will read out-of-bounds coefficients.
 *
 *  @tparam T  Supports float
 */

template <typename T> class Stft final : public VirtualOp {
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pX;          /*!< Pointer to input block [numChannels x numInputSamples], real */
   T *_pY;          /*!< Pointer to output spectrum [numChannels x numWindows x outBins], complex interleaved */
   T *_pWindow;     /*!< Pointer to analysis window [fftSize], real, caller-owned */
   T *_pState;      /*!< Pointer to overlap state [overlapBufSize x numChannels], caller-owned persistent */
   T *_pScratch;    /*!< Pointer to windowed frames half [numWindows x fftSize], first half of caller buffer */
   T *_pFftScratch; /*!< Pointer to FFT scratch / staging half [numWindows x fftSize], second half */

   uint32_t _fftSize;         /*!< FFT length */
   uint32_t _hopSize;         /*!< Hop size in samples */
   uint32_t _numChannels;     /*!< Number of independent channels */
   uint32_t _numInputSamples; /*!< Input samples per channel per exec() call (F) */
   uint32_t _numWindows;      /*!< Total output STFT frames per exec() call */
   uint32_t _overlapBufSize;  /*!< Unconsumed tail size (S) = F - firstFrameWindows * H */
   bool     _onesided;        /*!< true = N/2+1 bins, false = N bins */
   uint32_t _outBins;         /*!< Complex-interleaved floats per frame = onesided ? N+2 : 2N */
   std::unique_ptr<TISP::FFTLIB::RFFT1dBatched<T>, NodeDeleter> _rfftNode;   /*!< Internal RFFT node */
   std::unique_ptr<TISP::misc::BlockCopy<T>, NodeDeleter> _blockCopyOverlap; /*!< SE/SA copy for overlap state */
   std::unique_ptr<TISP::misc::BlockCopy<T>, NodeDeleter> _blockCopyInput; /*!< SE/SA copy for new input (F floats) */
   size_t _historyBufSize;

   using vec = typename c7x::make_full_vector<T>::type;

   __SE_TEMPLATE_v1 _seStaging     = __gen_SE_TEMPLATE_v1(); /*!< SE0: staging buffer reads (windowing) */
   __SE_TEMPLATE_v1 _seWindow      = __gen_SE_TEMPLATE_v1(); /*!< SE1: window coeff reads (windowing) */
   __SA_TEMPLATE_v1 _saOutput      = __gen_SA_TEMPLATE_v1(); /*!< SA0: windowed frame writes */
   uint32_t         _numWindowVecs = 0u;                     /*!< Vector iterations for windowing loop */

   __SE_TEMPLATE_v1 _seStateCopy      = __gen_SE_TEMPLATE_v1(); /*!< SE0: 1D read for state copy */
   __SA_TEMPLATE_v1 _saStateCopy      = __gen_SA_TEMPLATE_v1(); /*!< SA0: 2D write for state copy */
   __SE_TEMPLATE_v1 _seInputCopy      = __gen_SE_TEMPLATE_v1(); /*!< SE0: 1D read for input copy */
   __SA_TEMPLATE_v1 _saInputCopy      = __gen_SA_TEMPLATE_v1(); /*!< SA0: 2D write for input copy */
   __SE_TEMPLATE_v1 _seTailRead       = __gen_SE_TEMPLATE_v1(); /*!< SE0: 2D read for tail save */
   __SA_TEMPLATE_v1 _saTailWrite      = __gen_SA_TEMPLATE_v1(); /*!< SA0: 1D write for tail save */
   uint32_t         _numCopyStateVecs = 0u;                     /*!< Vector iterations for state copy */
   uint32_t         _numCopyInputVecs = 0u;                     /*!< Vector iterations for input copy */

   /*
    * two-pass windowing for execMultiWindow: boundary (staging) + direct (_pX).
    *
    * boundary windows straddle the state|input boundary and must be read from
    * the staging buffer (state + bridge input). direct windows lie entirely
    * within _pX and can be windowed in-place with no copy.
    *
    *   numBoundary = ceil(S/H)        — windows that touch any state data
    *   bridgeSize  = numBoundary * H  — input samples copied into staging
    *   directStart = bridgeSize - S   — offset into _pX for direct reads
    *
    * this avoids copying all F input samples when only bridgeSize << F are
    * needed in staging (e.g. 480 vs 16000 for N=512, H=160, F=16000).
    */
   __SE_TEMPLATE_v1 _seStagingDirect = __gen_SE_TEMPLATE_v1(); /*!< SE0: input reads for direct windows */
   __SE_TEMPLATE_v1 _seWindowDirect  = __gen_SE_TEMPLATE_v1(); /*!< SE1: window reads for direct windows */
   __SA_TEMPLATE_v1 _saOutputDirect  = __gen_SA_TEMPLATE_v1(); /*!< SA0: output writes for direct windows */
   uint32_t         _numBoundaryVecs = 0u;                     /*!< Vector iterations for boundary windowing pass */
   uint32_t         _numDirectVecs   = 0u;                     /*!< Vector iterations for direct windowing pass */
   uint32_t         _numBoundaryWindows = 0u;                  /*!< Windows needing staging (state + bridge input) */
   uint32_t         _numDirectWindows   = 0u;                  /*!< Windows reading directly from _pX */
   uint32_t         _bridgeSize         = 0u;                  /*!< Input samples copied to staging (bridge only, not all F) */
   uint32_t         _directStartInInput = 0u;                  /*!< Offset into _pX where direct window reads begin */

   void (Stft::*_execImpl)() noexcept; /*!< Exec dispatch: execMultiWindow or execBatchChannels */

   void execMultiWindow() noexcept;
   void execBatchChannels() noexcept;

   /* SE/SA template initializers — one per template member, no parameters */
   void initSeStaging() noexcept;
   void initSeWindow() noexcept;
   void initSaOutput() noexcept;
   void initSeStateCopy() noexcept;
   void initSaStateCopy() noexcept;
   void initSeInputCopy() noexcept;
   void initSaInputCopy() noexcept;
   void initSeTailRead() noexcept;
   void initSaTailWrite() noexcept;
   void initSeStagingDirect() noexcept;
   void initSeWindowDirect() noexcept;
   void initSaOutputDirect() noexcept;

 public:
   /**
    * @brief Creates an STFT node.
    *
    * @param [in]  *pX              : Pointer to input block [numChannels x numInputSamples].
    *                                 Real-valued, channel-major layout.
    *                                 Initializes @ref _pX.
    * @param [out] *pY              : Pointer to output complex spectrum
    *                                 [numChannels x numWindows x outBins].
    *                                 Complex interleaved (re, im) layout.
    *                                 Initializes @ref _pY.
    * @param [in]  *pWindow         : Pointer to pre-computed analysis window [fftSize].
    *                                 Caller-owned, persistent. When the analysis
    *                                 window is shorter than fftSize, use the
    *                                 center-padded windowGen overload to produce
    *                                 a zero-padded fftSize buffer.
    *                                 Initializes @ref _pWindow.
    * @param [in]  *pState          : Pointer to overlap state buffer
    *                                 [overlapBufSize x numChannels].
    *                                 Caller-owned, persistent, must be zero-initialized
    *                                 before the first exec() call.
    *                                 Initializes @ref _pState.
    * @param [in]  *pScratch        : Pointer to scratch buffer. Required size in
    *                                 floats is returned by calcScratchSize().
    *                                 Caller-owned, 64-byte aligned, may be reused
    *                                 between nodes. Split internally: first half
    *                                 for windowed frames (64-byte aligned boundary),
    *                                 second half shared between concat staging and
    *                                 FFT scratch.
    *                                 Initializes @ref _pScratch and @ref _pFftScratch.
    * @param [in]  fftSize          : FFT length.
    *                                 Initializes @ref _fftSize.
    * @param [in]  hopSize          : Hop size in samples.
    *                                 Initializes @ref _hopSize.
    * @param [in]  numChannels      : Number of independent channels.
    *                                 Initializes @ref _numChannels.
    * @param [in]  numInputSamples  : Number of input samples per channel per exec() call.
    *                                 Must be a multiple of hopSize and >= hopSize.
    *                                 Initializes @ref _numInputSamples.
    * @param [in]  onesided         : Output mode. true = half spectrum (N/2 + 1 bins),
    *                                 false = full spectrum (N bins).
    *                                 Initializes @ref _onesided.
    * @param [in]  errorCtx         : Error context for reporting constructor errors
    *
    * @pre  numInputSamples % hopSize == 0.
    * @pre  numInputSamples >= hopSize.
    * @pre  hopSize <= fftSize.
    * @pre  pScratch must be 64-byte aligned.
    *
    */
   Stft(T         *pX,
        T         *pY,
        T         *pWindow,
        T         *pState,
        T         *pScratch,
        uint32_t   fftSize,
        uint32_t   hopSize,
        uint32_t   numChannels,
        uint32_t   numInputSamples,
        bool       onesided,
        ErrorCtxt &errorCtx);

   /**
    * @brief Creates an STFT node with a name and ID.
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Stft(T          *pX,
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
        ErrorCtxt  &errorCtx);

   /**
    * @brief Set source and destination addresses for the operation.
    *
    * Updates pX (input) and/or pY (output) pointers. Used by the SuperNode
    * DMA framework to reassign L2 block pointers between ping-pong
    * iterations.
    *
    * @param [in] addrs : Structure containing source and destination addresses.
    *                     src[0] = pX, dst[0] = pY.
    * @param [in] mode  : Mode of address setting (input, output, or both).
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute streaming STFT.
    *
    * Dispatches to execMultiWindow (general per-channel path) or
    * execBatchChannels (single-window multi-channel batched path).
    * The dispatch target is set once in the constructor.
    *
    */
   void exec() noexcept override { (this->*_execImpl)(); }

   /**
    * @brief Get the name of the node.
    *
    * @retval const char* The name of the node.
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node.
    *
    * @retval uint32_t The ID of the node.
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node.
    *
    * @retval const char* The parameters of the node.
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
    * @brief Get the number of output STFT frames per exec() call.
    *
    * @retval uint32_t The number of output FFT frames.
    */
   uint32_t getNumWindows() const noexcept;

   /**
    * @brief Get the overlap buffer size (unconsumed tail).
    *
    * @retval uint32_t The overlap buffer size in samples.
    */
   uint32_t getOverlapBufSize() const noexcept;

   /**
    * @brief Compute number of output STFT frames before constructing the node.
    *
    * @param [in] fftSize         : FFT length.
    * @param [in] hopSize         : Hop size in samples.
    * @param [in] numInputSamples : Input samples per channel per exec() call.
    *
    * @retval uint32_t Number of output STFT frames per exec() call.
    */
   static uint32_t calcNumWindows(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples) noexcept;

   /**
    * @brief Compute overlap buffer size before constructing the node.
    *
    * @param [in] fftSize         : FFT length.
    * @param [in] hopSize         : Hop size in samples.
    * @param [in] numInputSamples : Input samples per channel per exec() call.
    *
    * @retval uint32_t Overlap buffer size in samples (per channel).
    */
   static uint32_t calcOverlapBufSize(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples) noexcept;

   /**
    * @brief Compute scratch buffer size before constructing the node.
    *
    * When numWindows == 1 and numChannels > 1, the node batches the FFT
    * across channels, requiring larger scratch (batchSize = numChannels
    * instead of numWindows).  The default numChannels = 1 preserves
    * backward compatibility for single-channel callers.
    *
    * @param [in] fftSize         : FFT length.
    * @param [in] hopSize         : Hop size in samples.
    * @param [in] numInputSamples : Input samples per channel per exec() call.
    * @param [in] numChannels     : Number of channels (default 1).
    *
    * @retval uint32_t Scratch buffer size in elements of type T.
    */
   static uint32_t
   calcScratchSize(uint32_t fftSize, uint32_t hopSize, uint32_t numInputSamples, uint32_t numChannels = 1u) noexcept;

   /**
    * @brief Compute total memory usage for all STFT buffers in kB.
    *
    * Sums input, output, window, state, and scratch buffer sizes.
    * Can be called before constructing the node to plan memory allocation.
    *
    * @param [in] fftSize         : FFT length.
    * @param [in] hopSize         : Hop size in samples.
    * @param [in] numChannels     : Number of independent channels.
    * @param [in] numInputSamples : Input samples per channel per exec() call.
    * @param [in] onesided        : Output mode (true = N/2+1 complex bins,
    *                               false = N complex bins).
    *
    * @retval float  Total memory in kB across all buffers.
    */
   static float calcTotalMemKB(uint32_t fftSize,
                               uint32_t hopSize,
                               uint32_t numChannels,
                               uint32_t numInputSamples,
                               bool     onesided) noexcept;

   /**
    * @brief Clean and reset all internal state and history buffers to zero
    *
    */
   void cleanInternalBuffers() noexcept override;

   /**
    * @brief Destructor.
    *
    */
   ~Stft() = default;
};

/** @} */

} /* namespace AUDIOLIB */
} /* namespace TISP */
