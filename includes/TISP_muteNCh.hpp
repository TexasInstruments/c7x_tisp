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
 * \class Mutench
 * @brief Provides node-level APIs for the AUDIOLIB_muteNCh kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel mutes or unmutes a multi-channel audio signal of the input buffer (interleaved or
 * non-interleaved) by applying a per-channel gain factor and writes the resulting values to the output buffer.
 * <br> The gain factor transitions between 1.0 (unmuted) and 0.0 (muted). To avoid
 * audible clicks, the transition can be smoothed over a specified `fadeTime`. The kernel
 * supports three distinct fade behaviors for each channel.
 *
 * <b>fadeType: </b>
 *
 *  | fadeType |   Operation  |
 *  |----------|--------------|
 *  |    0     |  Linear fade |
 *  |    1     |  Smooth fade |
 *  |    2     |   Hard fade  |
 *
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The MuteNCh node supports dynamic parameter updates via a robust runtime API. Users can query
 * or modify specific algorithmic parameters (such as mute state, fade time, and fade type)
 * without needing to reconstruct the node.
 *
 */
enum class MutenchParamId : uint16_t {
   kBypass      = 0, /**< kBypass: uint32_t bypass mode; `0` applies the mute, `1` copies input to output */
   kIsMuteValue = 1, /**< kIsMuteValue: Mute status values to set/get for each channel */
   kFadeTime    = 2, /**< kFadeTime: Duration of the fade-in or fade-out ramp in ms */
   kFadeType    = 3, /**< kFadeType: Type of fade curve to apply */
   kAll         = 4  /**< kAll: all tunable params as one @ref MutenchTunableParams payload */
};

/**
 * @brief Construction-time (non-tunable) parameters of the @ref Mutench node.
 *
 * @details Fixed for the node's lifetime; required.
 */
struct MutenchConfig {
   int32_t samplingRate = 48000; /*!< Sampling rate of the audio signal in Hz; must be greater than zero. */
};

/**
 * @brief Tunable parameters of the @ref Mutench node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref MutenchParamId::kAll payload of
 *          @ref Mutench::setParams / @ref Mutench::getParams.
 *
 * @tparam T : Node element type (float).
 */
template <typename T> struct MutenchTunableParams {
   uint32_t *pIsMute = nullptr; /*!< Caller-owned array, one flag per channel (`1`: engage mute (fade-out), `0`:
                                     disengage mute (fade-in)); required, must outlive the node. */
   float    fadeTime   = 0.0F;  /*!< Fade-in or fade-out ramp in milliseconds; `0` gives a hard (instant) mute. */
   uint32_t fadeType   = 0U;    /*!< Fade curve: `0` linear, `1` smooth, `2` hard. */
   uint32_t bypassMode = 0U;    /*!< `0`: apply the mute, `1`: copy input to output. */
};

template <typename T> class Mutench final : public VirtualOp {

   NodeInfo       _nodeInfo;     /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;           /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;          /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   size_t         _inChannels;   /*!< Number of channels (input width if interleaved, else height) */
   int32_t        _samplingRate; /*!< Audio sampling rate in Hz. */

   uint32_t _bypassMode; /*!< Bypass mode: `0` applies the mute, `1` copies input to output. */

   TISP::HandlePtr<char> _handle;     /*!< Handle to low level AUDIOLIB_muteNCh kernel. */
   TISP::HandlePtr<char> _setArgsBuf; /*!< Buffer to hold AUDIOLIB_muteNCh_SetArgs private member. */

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
    * @brief Creates an AUDIOLIB_muteNCh node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_muteNCh kernel. The channel
    *          count is the input @c width when interleaved, otherwise its @c height.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one buffer. Its @c pBuf is the
    *                                    initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one buffer with the same layout as
    *                                    the input.
    * @param [in] pConfig          : Construction-time parameters; required.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with a non-null @c pIsMute.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   Mutench(const TISP_BufDescList        *pInputs,
           const TISP_BufDescList        *pOutputs,
           const MutenchConfig      *pConfig,
           const MutenchTunableParams<T> *pDefaultTunableParams,
           const char                    *nodeName,
           uint32_t                       nodeId,
           ErrorCtxt                     &errorCtx);

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
    * This function sets the source and destination addresses for the operation
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

   ~Mutench() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
