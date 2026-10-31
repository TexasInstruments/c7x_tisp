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
 * \class Mute
 * @brief Provides node-level APIs for the AUDIOLIB_mute kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel mutes or unmutes a multi-channel audio signal of the input buffer (interleaved or
 * non-interleaved) by applying a gain factor and writes the resulting values to the output buffer.
 * <br> The gain factor transitions between 1.0 (unmuted) and 0.0 (muted). To avoid
 * audible clicks, the transition can be smoothed over a specified `fadeTime`. The kernel
 * supports four distinct fade behaviors.
 *
 * <b>fadeType: </b>
 *
 *  | fadeType |   Operation    |
 *  |----------|----------------|
 *  |    0     |  Linear fade   |
 *  |    1     |  Smooth fade   |
 *  |    2     |   Hard fade    |
 *  |    3     | Symmetric fade |
 *
 *  The symmetric fade ramps the gain geometrically (constant dB per second) between -200 dB and unity, so muting
 *  and unmuting take the same time, and the output is exactly zero once the gain has reached -200 dB.
 *
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The Mute node supports dynamic parameter updates via a robust runtime API. Users can query
 * or modify specific algorithmic parameters (such as mute state, fade time, and fade type)
 * without needing to reconstruct the node.
 *
 */
enum class MuteParamId : uint16_t {
   kIsMute   = 0, /**< kIsMute: uint32_t; `1` engages mute (fade-out), `0` disengages mute (fade-in) */
   kFadeTime = 1, /**< kFadeTime: Duration of the fade-in or fade-out ramp in ms */
   kFadeType = 2, /**< kFadeType: Type of fade curve to apply */
   kAll      = 3  /**< kAll: all tunable params as one @ref MuteTunableParams payload */
};

/**
 * @brief Construction-time (non-tunable) parameters of the @ref Mute node.
 *
 * @details Fixed for the node's lifetime; required.
 */
struct MuteConfig {
   int32_t samplingRate = 48000; /*!< Sampling rate of the audio signal in Hz; must be greater than zero. */
};

/**
 * @brief Tunable parameters of the @ref Mute node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref MuteParamId::kAll payload of
 *          @ref Mute::setParams / @ref Mute::getParams.
 *
 * @tparam T : Node element type (float).
 */
template <typename T> struct MuteTunableParams {
   uint32_t isMute   = 0U;   /*!< `1`: engage mute (fade-out), `0`: disengage mute (fade-in). */
   float    fadeTime = 0.0F; /*!< Fade-in or fade-out ramp in milliseconds; `0` gives a hard (instant) change. */
   uint32_t fadeType = 0U;   /*!< Fade curve: `0` linear, `1` smooth, `2` hard, `3` symmetric. */
};

template <typename T> class Mute final : public VirtualOp {

   NodeInfo       _nodeInfo;     /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;           /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;          /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   int32_t        _samplingRate; /*!< Audio sampling rate in Hz. */
   uint32_t       _isMute;       /*!< Mute switch: `1` engages mute, `0` disengages. */
   float          _fadeTime;     /*!< Fade ramp in milliseconds. */
   uint32_t       _fadeType;     /*!< Fade curve: `0` linear, `1` smooth, `2` hard, `3` symmetric. */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_mute kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_mute node.
    *
    * @details Validates the buffer descriptors and parameters, initializes the AUDIOLIB_mute kernel and applies
    *          the default tunable parameters with AUDIOLIB_mute_set.
    *
    * @param [in] pInputs              : Input buffer descriptors; exactly one buffer, interleaved or planar. Its
    *                                    @c pBuf is the initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs             : Output buffer descriptors; exactly one buffer with the same layout as the
    *                                    input. Its @c pBuf is the initial output pointer.
    * @param [in] pConfig         : Construction-time parameters; required.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required.
    * @param [in] nodeName             : Name of the node
    * @param [in] nodeId               : ID of the node
    * @param [in] errorCtx             : Error context for reporting constructor errors
    *
    * @throw None
    */
   Mute(const TISP_BufDescList     *pInputs,
        const TISP_BufDescList     *pOutputs,
        const MuteConfig           *pConfig,
        const MuteTunableParams<T> *pDefaultTunableParams,
        const char                 *nodeName,
        uint32_t                    nodeId,
        ErrorCtxt                  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: input buffer - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: output buffer - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * @param [in] addrs  : Structure containing source and destination addresses
    * @param [in] mode   : Mode of address setting (input, output, or both)
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Update runtime parameters with detailed error reporting
    *
    * @param [in] paramId      : Parameter ID to update (@ref MuteParamId)
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
    * @param [in] paramId      : Parameter ID to query (@ref MuteParamId)
    * @param [out] value       : Pointer to a buffer where parameters will be stored
    * @param [in,out] size     : Input: capacity of value in bytes. Output: bytes written or required.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true          : Parameters retrieved successfully
    * @retval false         : Failed to retrieve parameters
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

   /**
    * @brief Execute the operation
    */
   void exec() noexcept override;

   /**
    * @brief Get the name of the node
    *
    * @retval const char* The name of the node
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node
    *
    * @retval uint32_t The ID of the node
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node
    *
    * @retval const char* The parameters of the node
    */
   const char *getNodeParams() const noexcept override { return _nodeInfo.getNodeParams(); }

   /**
    * @brief Get the input and output buffer parameters of the node
    *
    * @retval const NodeBufParams& : The node's i/o buffer properties
    */
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Destructor
    */
   ~Mute() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
