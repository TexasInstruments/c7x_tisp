// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <cstddef>

namespace TISP {

namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 * \class Mixer
 * @brief Provides node-level APIs for the TISP_mixer using AUDIOLIB_inputAggregator and DSPLIB_matMul kernels.
 * @tparam T : Supports float and double.
 *
 * @details The Mixer kernel aggregates multiple audio input buffers from different sources
 * and applies coefficient-based mixing via matrix multiplication to produce mixed output channels.
 *
 * <b>Processing Pipeline Overview:</b>
 * @code
 * ┌─────────────────────────────────────────────────────────────────────────────────────────┐
 * │                              MIXER KERNEL PROCESSING FLOW                               │
 * └─────────────────────────────────────────────────────────────────────────────────────────┘
 *
 *    ┌─────────────────┐
 *    │  Input 0 (pX[0])│──┐     ┌──────────────────────────────┐
 *    │  [Ch0, Ch1]     │  │     │                              │
 *    └─────────────────┘  │     │   AUDIOLIB_inputAggregator   │     ┌─────────────────────┐
 *                         ├────►│                              │────►│   Scratch Buffer    │
 *    ┌─────────────────┐  │     │   Stacks all input channels  │     │   (pScratch)        │
 *    │  Input 1 (pX[1])│──┤     │   into a single contiguous   │     │   [TotalCh x N]     │
 *    │  [Ch2, Ch3]     │  │     │   planar buffer              │     │                     │
 *    └─────────────────┘  │     │                              │     │   ┌───────────────┐ │
 *                         │     └──────────────────────────────┘     │   │ Ch0: s0...sN-1│ │
 *    ┌─────────────────┐  │                                          │   │ Ch1: s0...sN-1│ │
 *    │  Input K (pX[K])│──┘                                          │   │ Ch2: s0...sN-1│ │
 *    │  [ChM-2, ChM-1] │                                             │   │ Ch3: s0...sN-1│ │
 *    └─────────────────┘                                             │   │     ...       │ │
 *                                                                    │   │ChM-1:s0...sN-1│ │
 *                                                                    │   └───────────────┘ │
 *                                                                    └──────────┬──────────┘
 *                                                                               │
 *                                                                               ▼
 *    ┌─────────────────────────────┐     ┌──────────────────────────────────────────────────┐
 *    │  Coefficient Matrix (pCoeff)│     │                                                  │
 *    │  [NumOutCh x TotalInCh]     │     │              DSPLIB_matMul                       │
 *    │                             │     │                                                  │
 *    │  ┌─────────────────────────┐│     │   Output = Coeff × Scratch                       │
 *    │  │ c00  c01  c02 ... c0M-1 ││     │   [NumOutCh x N] = [NumOutCh x M] × [M x N]      │
 *    │  │ c10  c11  c12 ... c1M-1 ││────►│                                                  │
 *    │  │ ...                     ││     │   Each output channel is a weighted sum of all   │
 *    │  │ cP-1,0 ... cP-1,M-1     ││     │   input channels based on coefficient row        │
 *    │  └─────────────────────────┘│     │                                                  │
 *    └─────────────────────────────┘     └──────────────────────────┬───────────────────────┘
 *                                                                   │
 *                                                                   ▼
 *                                        ┌──────────────────────────────────────────────────┐
 *                                        │              Output (Planar)                     │
 *                                        │              [NumOutCh x N]                      │
 *                                        │                                                  │
 *                                        │   ┌────────────────────────────────────────────┐ │
 *                                        │   │ OutCh0: w00*Ch0 + w01*Ch1 + ... + w0M*ChM-1│ │
 *                                        │   │ OutCh1: w10*Ch0 + w11*Ch1 + ... + w1M*ChM-1│ │
 *                                        │   │ ...                                        │ │
 *                                        │   │ OutChP-1: ...                              │ │
 *                                        │   └────────────────────────────────────────────┘ │
 *                                        └──────────────────────────┬───────────────────────┘
 *                                                                   │
 *                                                                   ▼
 *                                        ┌──────────────────────────────────────────────────┐
 *                                        │           (If isInterleave == 1)                 │
 *                                        │                                                  │
 *                                        │              DSPLIB_interleave                   │
 *                                        │                                                  │
 *                                        │   Converts planar [Channels x Samples] to        │
 *                                        │   interleaved [Samples x Channels] format        │
 *                                        └──────────────────────────┬───────────────────────┘
 *                                                                   │
 *                                                                   ▼
 *                                        ┌──────────────────────────────────────────────────┐
 *                                        │              Final Output (pY)                   │
 *                                        │   Format depends on isInterleave flag           │
 *                                        └──────────────────────────────────────────────────┘
 * @endcode
 *
 * <b>Data Format Support:</b>
 *
 *  | isInterleave | Input Data Layout             |  Output Data Layout            |
 *  |--------------|-------------------------------|--------------------------------|
 *  |      0       |  Planar (Channels x Samples)  |  Planar (Channels x Samples)   |
 *  |      1       |  Interleaved (Samples x Ch)   |  Interleaved (Samples x Ch)    |
 *
 * <b>Key Dimensions:</b>
 *
 *  | Parameter      | Description                                                    |
 *  |----------------|----------------------------------------------------------------|
 *  | numInputs      | Number of input audio sources (K)                              |
 *  | heightScratch  | Total input channels across all inputs (M = sum of all Ch)     |
 *  | widthCoeff     | Must equal heightScratch (total input channels M)              |
 *  | heightCoeff    | Number of output channels (P)                                  |
 *  | widthOut       | Number of samples per channel (N)                              |
 *  | heightOut      | Number of output channels (P)                                  |
 *
 * <b>Example (Planar Mode, isInterleave=0):</b>
 *
 * <b>Inputs:</b> 2 sources, 4 total channels, 3 samples each
 *
 *  Input 0 (2 channels):
 *  | Channel  |  s0  |  s1  |  s2  |
 *  |----------|------|------|------|
 *  |   Ch0    | 1.0  | 2.0  | 3.0  |
 *  |   Ch1    | 4.0  | 5.0  | 6.0  |
 *
 *  Input 1 (2 channels):
 *  | Channel  |  s0  |  s1  |  s2  |
 *  |----------|------|------|------|
 *  |   Ch2    | 0.5  | 1.5  | 2.5  |
 *  |   Ch3    | 3.5  | 4.5  | 5.5  |
 *
 * <b>After Input Aggregation (Scratch Buffer):</b> 4 channels stacked vertically
 *  | Channel  |  s0  |  s1  |  s2  |
 *  |----------|------|------|------|
 *  |   Ch0    | 1.0  | 2.0  | 3.0  |
 *  |   Ch1    | 4.0  | 5.0  | 6.0  |
 *  |   Ch2    | 0.5  | 1.5  | 2.5  |
 *  |   Ch3    | 3.5  | 4.5  | 5.5  |
 *
 * <b>Coefficient Matrix (pCoeff):</b> 2 outputs x 4 inputs
 *  | OutCh\\InCh |  Ch0  |  Ch1  |  Ch2  |  Ch3  |
 *  |-------------|-------|-------|-------|-------|
 *  |   OutCh0    |  0.5  |  0.5  |  0.0  |  0.0  |
 *  |   OutCh1    |  0.25 |  0.25 |  0.25 |  0.25 |
 *
 * <b>Output (pY):</b> 2 channels x 3 samples
 *  | Channel  |  s0                       |  s1                       |  s2                       |
 *  |----------|---------------------------|---------------------------|---------------------------|
 *  |  OutCh0  | 0.5*1.0+0.5*4.0 = 2.5     | 0.5*2.0+0.5*5.0 = 3.5     | 0.5*3.0+0.5*6.0 = 4.5     |
 *  |  OutCh1  | 0.25*(1+4+0.5+3.5) = 2.25 | 0.25*(2+5+1.5+4.5) = 3.25 | 0.25*(3+6+2.5+5.5) = 4.25 |
 *
 * <b>Mathematical Operation:</b>
 * @code
 *    Y[p][n] = Σ (Coeff[p][m] × Scratch[m][n])  for m = 0 to M-1
 *
 *    Where:
 *      - p = output channel index (0 to P-1)
 *      - n = sample index (0 to N-1)
 *      - m = input channel index (0 to M-1)
 *      - M = total input channels (heightScratch)
 *      - P = output channels (heightCoeff)
 *      - N = samples per channel (widthOut)
 * @endcode
 *
 */
template <typename T> class Mixer final : public VirtualOp {

   size_t                    _numInputs;        /*!< Number of input sources. */
   TISP_BufDesc2D           *_pX;               /*!< Pointer to input descriptors array. */
   T                        *_pY;               /*!< Pointer to output buffer. */
   T                        *_pYTemp;           /*!< Pointer to temporary output buffer for interleaving. */
   T                        *_pCoeff;           /*!< Pointer to mixing coefficient matrix. */
   T                        *_pScratch;         /*!< Pointer to scratch buffer for aggregated inputs. */
   std::unique_ptr<void*[]> _inputPtrs;        /*!< Array of pointers to input buffers. */
   TISP::HandlePtr<char>   _handle;           /*!< Handle to AUDIOLIB_inputAggregator kernel. */
   TISP::HandlePtr<char>   _handleMatMul;     /*!< Handle for DSPLIB matrix multiplication. */
   TISP::HandlePtr<char>   _handleInterleave; /*!< Handle for DSPLIB interleave operation. */

   size_t _widthOut;
   size_t _heightOut;
   size_t _strideOut;

   size_t _widthOutTemp;
   size_t _heightOutTemp;
   size_t _strideOutTemp;

   size_t _widthCoeff;
   size_t _heightCoeff;
   size_t _strideCoeff;
   size_t _heightScratch;

   bool __isInterleave;

   void (Mixer::*_execImpl)() noexcept; /*!< Function pointer bound at construction time for zero-overhead exec. */

   void _execNonInterleaved() noexcept;
   void _execInterleaved() noexcept;

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

 public:
   /**
    * @brief Creates an AUDIOLIB_mixer node.
    *
    * This constructor initializes a node for the AUDIOLIB_mixer kernel that
    * mixes multiple audio input sources into specified output channels using
    * a coefficient matrix.
    *
    * @param [in]  pX             : Pointer to array of input descriptors.
    *                               Initializes @ref _pX.
    * @param [in]  numInputs      : Number of input audio sources.
    * @param [in]  pMixerCoeff    : Pointer to mixing coefficient matrix [heightCoeff x widthCoeff].
    * @param [in]  widthCoeff     : Width of coefficient matrix (total input channels).
    * @param [in]  heightCoeff    : Height of coefficient matrix (number of output channels).
    * @param [in]  strideCoeff    : Stride of coefficient matrix in bytes.
    * @param [in]  pScratch       : Pointer to scratch buffer for intermediate aggregated data.
    * @param [in]  heightScratch  : Height of scratch buffer (total input channels).
    * @param [out] pOut           : Pointer to output buffer.
    *                               Initializes @ref _pY.
    * @param [in]  widthOut       : Width of output (number of samples).
    * @param [in]  heightOut      : Height of output (number of output channels).
    * @param [in]  strideOut      : Stride of output buffer in bytes.
    * @param [in]  pOutTemp       : Pointer to temporary output buffer (for interleaved output).
    * @param [in]  widthOutTemp   : Width of temporary output.
    * @param [in]  heightOutTemp  : Height of temporary output.
    * @param [in]  strideOutTemp  : Stride of temporary output buffer in bytes.
    * @param [in]  isInterleave   : Data format flag:
    *                             - `0`: Planar (Channels x Samples).
    *                             - `1`: Interleaved (Samples x Channels).
    * @param [in]  errorCtx       : Error context for reporting constructor errors
    *
    */
   Mixer(TISP_BufDesc2D *pX,
         size_t          numInputs,
         T              *pMixerCoeff,
         size_t          widthCoeff,
         size_t          heightCoeff,
         size_t          strideCoeff,
         T              *pScratch,
         size_t          heightScratch,
         T              *pOut,
         size_t          widthOut,
         size_t          heightOut,
         size_t          strideOut,
         T              *pOutTemp,
         size_t          widthOutTemp,
         size_t          heightOutTemp,
         size_t          strideOutTemp,
         size_t          isInterleave,
         ErrorCtxt      &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_mixer node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Mixer(TISP_BufDesc2D *pX,
         size_t          numInputs,
         T              *pMixerCoeff,
         size_t          widthCoeff,
         size_t          heightCoeff,
         size_t          strideCoeff,
         T              *pScratch,
         size_t          heightScratch,
         T              *pOut,
         size_t          widthOut,
         size_t          heightOut,
         size_t          strideOut,
         T              *pOutTemp,
         size_t          widthOutTemp,
         size_t          heightOutTemp,
         size_t          strideOutTemp,
         size_t          isInterleave,
         const char     *nodeName,
         uint32_t        nodeId,
         ErrorCtxt      &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0 to _numInputs-1: _pX[index].pBuf (input array)
    *
    * @param[in]  index : Index of the input buffer (0 to _numInputs-1)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pY (output data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (_numInputs: _pX)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pY)
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param [in] addrs  : Structure containing source and destination addresses
    * @param [in] mode   : Mode of address setting (input, output, or both)
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute the operation
    *
    * This function performs the actual operation.
    *
    */

   void exec() noexcept override;

   /**
    * @brief Get the name of the node
    *
    * This function returns the name of the node.
    *
    * @retval const char* The name of the node
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node
    *
    * This function returns the ID of the node.
    *
    * @retval uint32_t The ID of the node
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node
    *
    * This function returns a string containing the parameters of the node.
    *
    * @retval const char* The parameters of the node
    */
   const char *getNodeParams() const noexcept override { return _nodeInfo.getNodeParams(); }

   /**
    * @brief Get the input and output buffer parameters of the node
    *
    * This function returns reference to node's i/o buffer properties
    *
    * @retval const NodeBufParams& : A constant reference to the NodeBufParams representing node's i/o buffer properties
    */
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~Mixer() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
