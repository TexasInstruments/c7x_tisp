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
 * \class GainNChTrimSmoothing
 * @brief Provides node-level APIs for the AUDIOLIB_gainNChTrimSmoothing kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details Applies the gain masterGain * trimGains[ch] to every channel and writes the output. When a gain
 *          changes, the node fades to it: a symmetric ramp (constant dB per sample) or a one-pole smoother,
 *          continued across frames. Gains come from a caller-owned array
 *          (@ref GainNChTrimSmoothingTunableParams::pTrimGains), updatable via @ref setParams. Input and
 *          output share one layout (interleaved or non-interleaved) and hold at most 64 channels.
 **/

/**
 * <b>Runtime Control Philosophy:</b>
 * The GainNChTrimSmoothing node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class GainNChTrimSmoothingParamId : uint16_t {
   kTrimGains  = 0, /**< kTrimGains: trim gain to set/get for each channel */
   kMasterGain = 1, /**< kMasterGain: float master gain */
   kFadeTime   = 2, /**< kFadeTime: float fade time in ms */
   kFadeType   = 3, /**< kFadeType: uint32_t fade type; `0` symmetric, `1` smooth */
   kBypass     = 4, /**< kBypass: uint32_t bypass mode; `0` applies the gain, `1` copies input to output */
   kAll        = 5  /**< kAll: all tunable params as one @ref GainNChTrimSmoothingTunableParams payload */
};

/**
 * @brief Construction-time (non-tunable) parameters of the @ref GainNChTrimSmoothing node.
 *
 * @details Required.
 */
struct GainNChTrimSmoothingConfig {
   float samplingRate = 48000.0f; /*!< Sampling rate in Hz, used to convert fadeTime to samples. */
};

/**
 * @brief Tunable parameters of the @ref GainNChTrimSmoothing node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref GainNChTrimSmoothingParamId::kAll payload of
 *          @ref GainNChTrimSmoothing::setParams / @ref GainNChTrimSmoothing::getParams.
 *
 * @tparam T : Node element type (float).
 */
template <typename T> struct GainNChTrimSmoothingTunableParams {
   uint32_t bypassMode = 0U;      /*!< `0`: apply the gain, `1`: copy input to output. */
   float   *pTrimGains = nullptr; /*!< Caller-owned array, one linear trim gain per channel; required, must outlive
                                       the node. The target gain of channel ch is masterGain * pTrimGains[ch]. */
   float masterGain = 1.0f;       /*!< Linear gain applied to every channel on top of its trim gain. */
   float fadeTime   = 10.0f;      /*!< Fade time in ms: the ramp duration (symmetric) or the time constant (smooth);
                                       `0` applies the target at once. */
   uint32_t fadeType = 0U;        /*!< `0`: symmetric ramp (constant dB per sample), `1`: one-pole smoother. */
};

template <typename T> class GainNChTrimSmoothing final : public VirtualOp {

   NodeInfo                             _nodeInfo;      /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D                       _in;            /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D                       _out;           /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   GainNChTrimSmoothingConfig           _config;        /*!< Construction-time parameters */
   GainNChTrimSmoothingTunableParams<T> _tunableParams; /*!< Current tunable params; pTrimGains is the caller's array */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_gainNChTrimSmoothing kernel */

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
    * @brief Creates an AUDIOLIB_gainNChTrimSmoothing node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_gainNChTrimSmoothing kernel, with
    *          every channel at rest on its target gain. The channel count is the input @c width when interleaved,
    *          otherwise its @c height.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one buffer. Its @c pBuf is the
    *                                    initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one buffer with the same layout as
    *                                    the input.
    * @param [in] pConfig          : Construction-time parameters; required.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with a non-null @c pTrimGains.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   GainNChTrimSmoothing(const TISP_BufDescList                     *pInputs,
                        const TISP_BufDescList                     *pOutputs,
                        const GainNChTrimSmoothingConfig           *pConfig,
                        const GainNChTrimSmoothingTunableParams<T> *pDefaultTunableParams,
                        const char                                 *nodeName,
                        uint32_t                                    nodeId,
                        ErrorCtxt                                  &errorCtx);

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
    * @retval    size_t : The number of input buffers (1: input buffer)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: output buffer)
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param [in] addrs : Structure containing source and destination addresses
    * @param [in] mode  : Mode of address setting (input, output, or both)
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Update runtime parameters with detailed error reporting
    *
    * Updates the specified runtime parameter by ID with the data provided in the value buffer.
    * The node strictly validates that the provided `size` argument matches or exceeds the
    * expected byte size for the given parameter before mapping the state into the active node memory.
    *
    * @param [in] paramId  : Parameter ID to update
    * @param [in] value    : Pointer to a buffer containing the new parameter value
    * @param [in] size     : The size in bytes of the payload
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
    * @param [in] paramId  : Parameter ID to query
    * @param [out] value    : Pointer to a buffer where parameters will be stored
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
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */
   ~GainNChTrimSmoothing() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
