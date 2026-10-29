// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"

namespace TISP {

namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 * \class MultiplexerSmooth
 * @brief Provides node-level APIs for the AUDIOLIB_multiplexerSmooth kernel from AUDIOLIB.
 * @tparam T : Supports float, double.
 *
 * @details This kernel routes selected channels from one of the multiple input sources
 * to the output buffer, applying a smooth crossfade transition when switching between sources.
 *
 * <b>Processing Pipeline Overview:</b>
 * @code
 * ┌─────────────────────────────────────────────────────────────────────────────────────────┐
 * │                     MULTIPLEXER SMOOTH PROCESSING FLOW                                  │
 * └─────────────────────────────────────────────────────────────────────────────────────────┘
 *
 *    ┌──────────────────────┐
 *    │  Input Sources (pX0) │
 *    │  [Multiple sources]  │
 *    └──────────┬───────────┘
 *               │
 *               ▼
 *    ┌──────────────────────┐
 *    │   Input Selection    │
 *    │   (Based on srcIdx)  │
 *    └──────────┬───────────┘
 *               │
 *               ▼
 *    ┌──────────────────────┐     ┌──────────────────────┐
 *    │   Current Input      │◄────│ Previous Input State │
 *    │   (Selected Source)  │     │ (SmoothedSampleBuff) │
 *    └──────────┬───────────┘     └──────────┬───────────┘
 *               │                            │
 *               ▼                            ▼
 *    ┌──────────────────────┐     ┌──────────────────────┐
 *    │   Fade In Gains      │     │   Fade Out Gains     │
 *    │   (Sine Table)       │     │   (Cosine Table)     │
 *    └──────────┬───────────┘     └──────────┬───────────┘
 *               │                            │
 *               ▼                            ▼
 *    ┌──────────────────────────────────────────────────┐
 *    │              Crossfade / Smoothing               │
 *    │                                                  │
 *    │   Output = (PrevInput * Cos) + (CurrInput * Sin) │
 *    │   (Applied only for 'fadeTime' duration)         │
 *    └────────────────────────┬─────────────────────────┘
 *                             │
 *                             ▼
 *                  ┌──────────────────────┐
 *                  │    Output (pY)       │
 *                  └──────────────────────┘
 * @endcode
 *
 * <b>Data Format Support:</b>
 *
 *  | isInterleave | Input Data Layout (pX)        | Description                                    |
 *  |--------------|-------------------------------|------------------------------------------------|
 *  |      0       |  Planar (Channels x Samples)  |  Each row is a channel, each column a sample.  |
 *  |      1       |  Interleaved (Samples x Ch)   |  Each row is a sample time, across channels.   |
 *
 * <b>Key Dimensions & Memory:</b>
 *
 *  | Parameter      | Value / Constraint                                             |
 *  |----------------|----------------------------------------------------------------|
 *  | SmoothedSamples| `(sampleRate * fadeTime) / 1000`                               |
 *  | Scratch (pWs)  | Must be at least `2 * (SmoothedSamples * numChannels)` elements|
 *  | History Buffer | Must be at least `(SmoothedSamples * numChannels)` elements    |
 *
 * <b>Crossfade Logic:</b>
 * When the source index (`srcIdx`) changes, the kernel performs a crossfade between the
 * previously selected input source and the newly selected source.
 *
 * - **Unchanged Source:** If `srcIdx` remains the same, a direct block copy is performed.
 * - **Changed Source:**
 *   - The first `SmoothedSamples` (determined by `fadeTime`) are computed as a weighted sum:
 *     `Output[i] = (PreviousInput[i] * CosGain[i]) + (CurrentInput[i] * SinGain[i])`
 *   - The remaining samples in the block are copied directly from the Current Input.
 *   - The `PreviousInput` state is updated for the next frame.
 *
 * <b>Configuration & Parameters:</b>
 *
 * | Parameter      | Configuration Time | Description |
 * |----------------|--------------------|------------------------------------------------------------------------------|
 * | fadeTime       | Instance Creation  | Duration of the crossfade in ms. <b>FIXED</b> at creation time.          |
 * | srcIdx         | Runtime            | Index of the input source to route. <b>ONLY</b> runtime changeable
 * parameter.| | sampleRate     | Instance Creation  | Sample rate of the audio stream. |
 *
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The MultiplexerSmooth node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class MultiplexersmoothParamId : uint16_t {
   kSrcIndex = 0 /**< uint32_t: Index to the required source */
};

template <typename T> class MultiplexerSmooth final : public VirtualOp {

   T *_pX0;                 /*!< Pointer to input matrix. */
   T *_pWs;                 /*!< Pointer to workspace memory. */
   T *_pSmoothedSampleBuff; /*!< Pointer to buffer holding data from previous input buffer. */
   T *_pGainFadeIn;         /*!< Pointer to sin gain buffer. */
   T *_pGainFadeOut;        /*!< Pointer to cos gain buffer. */
   T *_pY;                  /*!< Pointer to output matrix. */

   size_t   _width;
   size_t   _height;
   size_t   _inPitchBytes;
   size_t   _outPitchBytes;
   size_t   _isInterleave;
   size_t   _smoothedSamples;
   size_t   _curentIdx;
   size_t   _prevIdx;
   uint32_t _maxInputs;

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   TISP::HandlePtr<char> _handleCrossFader; /*!< Handle to low level AUDIOLIB_multiplexerSmooth kernel. */
   TISP::HandlePtr<char> _handleBlkCopy;    /*!< Handle to low level DSPLIB_blkCopy2D kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_multiplexerSmooth node.
    *
    * This constructor initializes a node for the AUDIOLIB_multiplexerSmooth kernel.
    *
    * @param [in]  pX0                 : Pointer to the input matrix.
    * @param [in]  pSmoothedSampleBuff : Pointer to buffer holding data from previous input buffer.
    * @param [in]  pGainFadeIn         : Pointer to sin gain buffer.
    * @param [in]  pGainFadeOut        : Pointer to cos gain buffer.
    * @param [out] pY                  : Pointer to the output matrix.
    * @param [out] pWs                 : Pointer to the workspace memory. Size should be (SmoothedSamples * nChannels)
    * * 2.
    * @param [in]  maxInputs           : Maximum input sources in pX0.
    * @param [in]  widthIn             : Width of input matrix.
    * @param [in]  heightIn            : Height of input matrix.
    * @param [in]  inPitchBytes        : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes       : Pitch of output matrix in bytes.
    * @param [in]  isInterleave        : Indicates whether the input matrix data is interleaved:
    *                                  - `0`: Non-interleaved (Rows represent channels, columns represent samples).
    *                                  - `1`: Interleaved (Rows represent samples, columns represent channels).
    * @param [in]  currentIdx          : Index of the input source to route.
    * @param [in]  sampleRate          : Sample rate of input signal.
    * @param [in]  fadeTime            : Required fadeTime in ms. This should be <= to width (numSamples) of the input
    * block.
    * @param [in]  errorCtx            : Error context for reporting constructor errors
    *
    */

   MultiplexerSmooth(T         *pX0,
                     T         *pSmoothedSampleBuff,
                     T         *pGainFadeIn,
                     T         *pGainFadeOut,
                     T         *pY,
                     T         *pWs,
                     uint32_t   maxInputs,
                     size_t     widthIn,
                     size_t     heightIn,
                     size_t     inPitchBytes,
                     size_t     outPitchBytes,
                     size_t     isInterleave,
                     size_t     currentIdx,
                     size_t     sampleRate,
                     float      fadeTime,
                     ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_multiplexerSmooth node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */

   MultiplexerSmooth(T          *pX0,
                     T          *pSmoothedSampleBuff,
                     T          *pGainFadeIn,
                     T          *pGainFadeOut,
                     T          *pY,
                     T          *pWs,
                     uint32_t    maxInputs,
                     size_t      widthIn,
                     size_t      heightIn,
                     size_t      inPitchBytes,
                     size_t      outPitchBytes,
                     size_t      isInterleave,
                     size_t      currentIdx,
                     size_t      sampleRate,
                     float       fadeTime,
                     const char *nodeName,
                     uint32_t    nodeId,
                     ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pX0 (input data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
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
    * @retval    size_t : The number of input buffers (1: _pX0)
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
    * @brief Update runtime parameters with detailed error reporting
    *
    * Updates the specified runtime parameter by ID with the data provided in the value buffer.
    * The node strictly validates that the provided `size` argument matches or exceeds the
    * expected byte size for the given parameter before mapping the state into the active node memory.
    *
    * @param [in] paramId      : Parameter ID to update
    * @param [in] value        : Pointer to a buffer containing the new parameter value
    * @param [in] size         : The size in bytes of the payload
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true         : Parameters updated successfully
    * @retval false        : Failed to update parameters
    */
   bool setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept override;

   /**
    * @brief Retrieve current runtime parameters with detailed error reporting
    *
    * This API fetches the current live state from the node for the specified parameter ID
    * and writes it into the provided buffer.
    *
    * If the user provides a buffer, it populates the requested data. If the buffer is too small
    * (or `value` is `nullptr`), it safely fails (returns `false`) but injects the exact required
    * byte capacity into `*size`, allowing the application to re-allocate precisely.
    *
    * @param [in] paramId      : Parameter ID to query
    * @param [out] value       : Pointer to a buffer where parameters will be stored
    * @param [in,out] size     : Input: Capacity of value buffer in bytes. Output: Actual bytes written or required.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true          : Parameters retrieved successfully
    * @retval false         : Failed to retrieve parameters
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

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
    * @brief Clean and reset all internal history buffers to zero when smoothedSamples are not equal to zero.
    *
    * @retval None
    * @throw None
    */
   void cleanInternalBuffers() noexcept override;

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~MultiplexerSmooth() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
