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
 * \class DelayNChannel
 * @brief Provides node-level APIs for the AUDIOLIB_delayNChannel kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel copies each channel from the input buffer
 * (interleaved or non-interleaved) to the output buffer based on a
 * given array of delay sizes (@ref DelayNChannelConfig::pDelaySize). <br>For each channel, corresponding delay
 * size elements of zeros are stored in the output and then the corresponding input channel is stored in the output.
 *
 * <b>Example (Non-interleaved data): </b>
 *
 * <b>Input</b>: No. of samples = N; No. of channels = M.
 *  | Channel/Sample | s0  | s1  | ... | sN-3  | sN-2  | sN-1  |
 *  |--------------- |-----|-----|-----|-------|-------|-------|
 *  |      ch0       | 100 | 320 | ... |  430  |  520  |  701  |
 *  |      ch1       | 120 | 220 | ... |  630  |  840  |  620  |
 *  |      .         |  .  |  .  | ... |   .   |   .   |    .  |
 *  |      .         |  .  |  .  | ... |   .   |   .   |   .   |
 *  |      .         |  .  |  .  | ... |   .   |   .   |   .   |
 *  |      chM       | 980 |1050 | ... | 1024  |  240  |  156  |
 *
 * <b>mode</b> (@ref DelayNChannelConfig::mode): 0 <br>
 * <b>delay sizes array</b> (@ref DelayNChannelConfig::pDelaySize): `{1, 2,...., 0}` <br>
 *
 * <b>Output</b>: No. of samples = N; No. of channels = M.
 *  | Channel/Sample | s0  | s1  |  s2 | ... | sN-1  |
 *  |--------------- |-----|-----|-----|-----|-------|
 *  |      ch0       |  0  | 100 | 320 | ... |  520  |
 *  |      ch1       |  0  |  0  | 120 | ... |  630  |
 *  |      .         |  .  |  .  | ... | ... |   .   |
 *  |      .         |  .  |  .  | ... | ... |   .   |
 *  |      .         |  .  |  .  | ... | ... |   .   |
 *  |      chM       | 980 |1050 |  .  | ... |  156  |
 *
 */

/**
 * @brief Construction-time (non-tunable) parameters of the @ref DelayNChannel node.
 *
 * @details Required; fixed for the node's lifetime.
 */
struct DelayNChannelConfig {
   uint32_t  mode       = 0U;      /*!< `0`: linear delay, `1`: circular delay. */
   uint32_t *pDelaySize = nullptr; /*!< Caller-owned array, one delay in samples per channel; required. Copied into
                                        the kernel at construction. */
};

template <typename T> class DelayNChannel final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _state;    /*!< Delay line descriptor, one row per channel */
   T             *_scratch;  /*!< Scratch matrix (channels x samples, packed rows) */
   uint32_t       _mode;     /*!< `0`: linear delay, `1`: circular delay */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_delayNChannel kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_delayNChannel node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_delayNChannel kernel, then clears the
    *          delay line. The channel count is the input @c width when interleaved, otherwise its @c height.
    *
    * @param [in] pInputs      : Input buffer descriptors; exactly one buffer, interleaved or non-interleaved. Its
    *                            @c pBuf is the initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs     : Output buffer descriptors; exactly one buffer with the input's layout, width and
    *                            height.
    * @param [in] pStates      : State buffer descriptors; exactly one delay line with one row per channel (at least
    *                            as many rows as channels), whatever the data layout. Its @c width is the maximum delay
    *                            (linear mode) or the circular buffer length; the kernel takes the delay buffer length
    *                            from @c pitchBytes. Its @c isInterleave is not used.
    * @param [in] pScratch     : Scratch buffer; required.
    * @param [in] scratchBytes : Size of @c pScratch in bytes; at least channels x samples elements.
    * @param [in] pConfig : Construction-time parameters; required, with a non-null @c pDelaySize.
    * @param [in] nodeName     : Name of the node
    * @param [in] nodeId       : ID of the node
    * @param [in] errorCtx     : Error context for reporting constructor errors
    *
    * @throw None
    */
   DelayNChannel(const TISP_BufDescList         *pInputs,
                 const TISP_BufDescList         *pOutputs,
                 const TISP_BufDescList         *pStates,
                 void                           *pScratch,
                 size_t                          scratchBytes,
                 const DelayNChannelConfig *pConfig,
                 const char                     *nodeName,
                 uint32_t                        nodeId,
                 ErrorCtxt                      &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: input buffer (@ref _in) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: output buffer (@ref _out) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1)
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
    * @brief Clean and reset all internal state and history buffers to zero
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

   ~DelayNChannel() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
