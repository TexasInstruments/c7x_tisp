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
 * @tparam T : Supports float and double.
 *
 * @details This kernel mutes or unmutes a multi-channel audio signal of the input buffer pX (interleaved or
 * non-interleaved) by applying a per-channel gain factor and writes the resulting values to the output buffer pY.
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
   kIsMuteValue = 0, /**< kIsMuteValue: Mute status values to set/get for each channel */
   kFadeTime    = 1, /**< kFadeTime: Duration of the fade-in or fade-out ramp in ms */
   kFadeType    = 2, /**< kFadeType: Type of fade curve to apply */
   kBypass      = 3  /**< kBypass: uint32_t bypass mode; `0` applies the mute, `1` copies input to output */
};

template <typename T> class Mutench final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input matrix. */
   T       *_pY;       /*!< Pointer to output matrix. */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   size_t  _width;         /*!< Width of the input/output matrix. */
   size_t  _height;        /*!< Height of the input/output matrix. */
   size_t  _inChannels;    /*!< Number of audio channels. */
   size_t  _inPitchBytes;  /*!< Pitch of the input matrix in bytes. */
   size_t  _outPitchBytes; /*!< Pitch of the output matrix in bytes. */
   size_t  _isInterleaved; /*!< Data layout: 1 = interleaved, 0 = non-interleaved. */
   int32_t _samplingRate;  /*!< Audio sampling rate in Hz. */

   uint32_t _bypassMode; /*!< Bypass mode: `0` applies the mute, `1` copies input to output. */

   TISP::HandlePtr<char> _handle;     /*!< Handle to low level AUDIOLIB_muteNCh kernel. */
   TISP::HandlePtr<char> _setArgsBuf; /*!< Buffer to hold AUDIOLIB_muteNCh_SetArgs private member. */

   /**
    * @brief Copies @ref _height rows of @ref _width elements from @ref _pX to @ref _pY, honouring
    *        @ref _inPitchBytes and @ref _outPitchBytes.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

 public:
   /**
    * @brief Creates an AUDIOLIB_muteNCh node.
    *
    * This constructor initializes a node for the AUDIOLIB_muteNCh kernel.
    *
    * @param [in]  *pX            : Pointer to the input matrix.
    *                               Initializes @ref _pX.
    * @param [out] *pY            : Pointer to the output matrix.
    *                               Initializes @ref _pY.
    * @param [in]  width          : Width of input matrix.
    * @param [in]  height         : Height of input matrix.
    * @param [in]  inChannels     : Number of input channels.
    * @param [in]  inPitchBytes   : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes  : Pitch of output matrix in bytes.
    * @param [in]  isInterleaved  : Indicates whether the input matrix data is interleaved:
    *                             - `0`: Non-interleaved (Rows represent channels, columns represent samples).
    *                             - `1`: Interleaved (Rows represent samples, columns represent channels).
    * @param [in]  samplingRate   : Sampling rate of an audio signal (in Hz).
    * @param [in]  isMute         : Per-channel control switch for muting (int32 array, one flag per channel).
    *                             - '1': Engage mute (fade-out).
    *                             - '0': Disengage mute (fade-in).
    * @param [in]  fadeTime       : Duration of the fade-in or fade-out ramp in milliseconds. A value of 0 results in a
    *                               hard (instant) mute.
    * @param [in]  fadeType       : Type of fade curve to apply.
    *                             - '0': Linear fade.
    *                             - '1': Smooth fade.
    *                             - '2': Hard fade.
    * @param [in]  errorCtx       : Error context for reporting constructor errors
    *
    */
   Mutench(T         *pX,
           T         *pY,
           size_t     width,
           size_t     height,
           size_t     inChannels,
           size_t     inPitchBytes,
           size_t     outPitchBytes,
           size_t     isInterleaved,
           int32_t    samplingRate,
           uint32_t  *isMute,
           float      fadeTime,
           uint32_t   fadeType,
           ErrorCtxt &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_muteNCh node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Mutench(T          *pX,
           T          *pY,
           size_t      width,
           size_t      height,
           size_t      inChannels,
           size_t      inPitchBytes,
           size_t      outPitchBytes,
           size_t      isInterleaved,
           int32_t     samplingRate,
           uint32_t   *isMute,
           float       fadeTime,
           uint32_t    fadeType,
           const char *nodeName,
           uint32_t    nodeId,
           ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pX (input data matrix) - managed by setAddr()
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
