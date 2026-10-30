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
 * \class GainNChTrim
 * @brief Provides node-level APIs for the AUDIOLIB_gainNChTrim kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel applies a specified gain factor to each channel of the input buffer
 * (interleaved or non-interleaved), multiplies the obtained values by a master gain value, and writes
 * the resulting values to the output buffer. The per-channel gains come from a caller-owned array
 * (@ref GainNChTrimTunableParams::pGains) and the master gain from a caller-owned value
 * (@ref GainNChTrimTunableParams::pMasterGain), both updatable via @ref setParams.
 *
 * <b>Example (Interleaved data): </b>
 *
 * <b>Input</b>: No. of samples = N; No. of channels = M.
 *  | SampleChannel | ch0 | ch1 | ... | chM-1 |
 *  |--------------- |-----|-----|-----|-------|
 *  |      s0        | 100 | 320 | ... |  430  |
 *  |      s1        | 120 | 220 | ... |  630  |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |     sN-1       | 980 |1050 | ... | 1024  |
 *
 * <b>Gain values</b> (@ref GainNChTrimTunableParams::pGains): `{1, 3, ..., 2}` <br>
 * <b>Master gain value</b> (@ref GainNChTrimTunableParams::pMasterGain): 4 <br>
 *
 * <b>Operation:</b>
 * output[s0][ch0] = (input[s0][ch0]*gain[ch0])*masterGain
 *
 * <b>Output</b>: No. of samples = N; No. of channels = M.
 *  | SampleChannel |  Ch0 |  Ch1 | ... | ChM-1 |
 *  |--------------- |------|------|-----|-------|
 *  |      s0        |  400 | 3840 | ... | 3440  |
 *  |      s1        |  480 | 2640 | ... | 5040  |
 *  |      .         |   .  |   .  | ... |   .   |
 *  |      .         |   .  |   .  | ... |   .   |
 *  |      .         |   .  |   .  | ... |   .   |
 *  |     sN-1       | 3920 |12600 | ... | 8192  |
 *
 * <b>Configuration & Parameters:</b>
 *
 * | Parameter    | Configuration Time | Description                                                         |
 * |--------------|--------------------|---------------------------------------------------------------------|
 * | Channels     | Instance Creation  | Input width if interleaved, else height (gain array size). <b>FIXED</b>.|
 * | Gain values  | Runtime            | Gain factor for each channel.                                       |
 * | Master gain  | Runtime            | Master gain factor.                                                 |
 * | Bypass       | Runtime            | Copies input to output, leaving the gain unapplied.                 |
 *
 **/

/**
 * <b>Runtime Control Philosophy:</b>
 * The GainNChTrim node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class GainNChTrimParamId : uint16_t {
   kBypass     = 0, /**< kBypass: uint32_t bypass mode; `0` applies the gain, `1` copies input to output */
   kGainValues = 1, /**< kGainValues: Gain values to set/get for each channel */
   kMasterGain = 2, /**< kMasterGain: Master gain scalar (float) to set/get */
   kAll        = 3  /**< kAll: all tunable params as one @ref GainNChTrimTunableParams payload */
};

/**
 * @brief Tunable parameters of the @ref GainNChTrim node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref GainNChTrimParamId::kAll payload of
 *          @ref GainNChTrim::setParams / @ref GainNChTrim::getParams.
 *
 * @tparam T : Node element type (float or double).
 */
template <typename T> struct GainNChTrimTunableParams {
   T       *pGains      = nullptr; /*!< Caller-owned array, one gain per channel; required, must outlive the node. */
   T       *pMasterGain = nullptr; /*!< Caller-owned master gain value; required, must outlive the node. */
   uint32_t bypassMode  = 0U;      /*!< `0`: apply the gain, `1`: copy input to output. */
};

template <typename T> class GainNChTrim final : public VirtualOp {

   NodeInfo       _nodeInfo;    /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;          /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;         /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   size_t         _numChannels; /*!< Number of channels (input width if interleaved, else height) */
   uint32_t       _bypassMode;  /*!< Bypass mode: `0` applies the gain, `1` copies input to output */

   T                    *_gains;      /*!< Caller-owned gain array (numChannels), used live by exec and setParams */
   T                    *_masterGain; /*!< Caller-owned master gain value, used live by exec and setParams */
   TISP::HandlePtr<char> _handle;     /*!< Handle to low level AUDIOLIB_gainNChTrim kernel */

   /**
    * @brief Copies the input buffer described by @ref _in to the output buffer described by @ref _out,
    *        row by row, honouring both row pitches.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

 public:
   /**
    * @brief Creates an AUDIOLIB_gainNChTrim node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_gainNChTrim kernel. The channel
    *          count is the input @c width when interleaved (rows are samples, columns are channels), otherwise
    *          its @c height (rows are channels, columns are samples).
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one buffer. Its @c pBuf is the
    *                                    initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one buffer with the same layout as
    *                                    the input.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with non-null @c pGains and
    *                                    @c pMasterGain.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   GainNChTrim(const TISP_BufDescList            *pInputs,
               const TISP_BufDescList            *pOutputs,
               const GainNChTrimTunableParams<T> *pDefaultTunableParams,
               const char                        *nodeName,
               uint32_t                           nodeId,
               ErrorCtxt                         &errorCtx);

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
    * @brief Update runtime parameters with detailed error reporting
    *
    * Updates the specified runtime parameter by ID with the data provided in the value buffer.
    * The node strictly validates that the provided `size` argument matches or exceeds the
    * expected byte size for the given parameter before mapping the state into the active node memory.
    *
    * @param [in] paramId      : Parameter ID to update (@ref GainNChTrimParamId)
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
    * @param [in] paramId      : Parameter ID to query (@ref GainNChTrimParamId)
    * @param [out] value       : Pointer to a buffer where parameters will be stored
    * @param [in,out] size     : Input: Capacity of value buffer in bytes. Output: Actual bytes written or required.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true          : Parameters retrieved successfully
    * @retval false         : Failed to retrieve parameters
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

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

   ~GainNChTrim() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
