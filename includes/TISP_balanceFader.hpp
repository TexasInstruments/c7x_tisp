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
 * \class BalanceFader
 * @brief Provides node-level APIs for the AUDIOLIB_balanceFader kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel adjusts the sound distribution in an audio signal of the input buffer pX (interleaved or
 * non-interleaved) by applying a corresponding gain factor and writes the resulting values to the output buffer pY.
 * <br> The gain factor for a channel is calculated based on given values of the following parameters:
 * - balance,
 * - fade,
 * - array of channel config values,
 * - lfeBalanceGainFactor,
 * - lfeFaderGainFactor,
 * - sideGainFactor,
 * - lfeBalanceMode,
 * - lfeFaderMode.
 *
 * <b>Runtime Control Philosophy:</b>
 * The BalanceFader node supports dynamic parameter updates via a robust runtime API. Users can query
 * or modify specific algorithmic parameters (such as balance, fade, LFE settings, and side gain)
 * without needing to reconstruct the node.
 *
 */

enum class BalanceFaderParamId : uint16_t {
   kBalance              = 0, /**< kBalance: Left/Right balance control */
   kFade                 = 1, /**< kFade: Front/Rear fader control */
   kLfeBalanceGainFactor = 2, /**< kLfeBalanceGainFactor: LFE balance gain factor */
   kLfeFaderGainFactor   = 3, /**< kLfeFaderGainFactor: LFE fader gain factor */
   kSideGainFactor       = 4, /**< kSideGainFactor: Side gain factor */
   kLfeBalanceMode       = 5, /**< kLfeBalanceMode: LFE balance mode */
   kLfeFaderMode         = 6, /**< kLfeFaderMode: LFE fader mode */
   kChannelConfig        = 7, /**< kChannelConfig: Speaker type per channel array */
   kBypass               = 8  /**< kBypass: uint32_t bypass mode; `0` applies the gain, `1` copies input to output */
};

template <typename T> class BalanceFader final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input matrix. */
   T       *_pG;       /*!< Pointer to input gain array. */
   T       *_pY;       /*!< Pointer to output matrix. */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   size_t   _widthIn;              /*!< Width (samples or channels per isInterleave) of the input matrix. */
   size_t   _heightIn;             /*!< Height of the input matrix. */
   size_t   _gWidthIn;             /*!< Number of gain values (one per channel). */
   size_t   _inPitchBytes;         /*!< Pitch in bytes of the input matrix. */
   size_t   _outPitchBytes;        /*!< Pitch in bytes of the output matrix. */
   size_t   _isInterleave;         /*!< 0 = non-interleaved, 1 = interleaved. */
   float    _balance;              /*!< Left/Right control, range [-1, 1]. */
   float    _fade;                 /*!< Front/Rear control, range [-1, 1]. */
   int32_t *_channelConfig;        /*!< Per-channel speaker-type configuration array. */
   float    _lfeBalanceGainFactor; /*!< LFE balance gain factor, range [0, 1]. */
   float    _lfeFaderGainFactor;   /*!< LFE fader gain factor, range [0, 1]. */
   float    _sideGainFactor;       /*!< Side gain factor, range [0, 1]. */
   int32_t  _lfeBalanceMode;       /*!< LFE response mode to balance control. */
   int32_t  _lfeFaderMode;         /*!< LFE response mode to fader control. */

   uint32_t _bypassMode; /*!< Bypass mode: `0` applies the gain, `1` copies input to output. */

   TISP::HandlePtr<char> _handle;     /*!< Handle to low level AUDIOLIB_balanceFader kernel. */
   TISP::HandlePtr<char> _setArgsBuf; /*!< Buffer to hold AUDIOLIB_balanceFader_SetArgs private member. */

   /**
    * @brief Copies @ref _heightIn rows of @ref _widthIn elements from @ref _pX to @ref _pY, honouring
    *        @ref _inPitchBytes and @ref _outPitchBytes.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

 public:
   /**
    * @brief Creates an AUDIOLIB_balanceFader node.
    *
    * This constructor initializes a node for the AUDIOLIB_balanceFader kernel.
    *
    * @param [in]  *pX                  : Pointer to the input matrix.
    *                                     Initializes @ref _pX.
    * @param [in]  *pG                  : Pointer to the input gain values array.
    *                                     Initializes @ref _pG.
    * @param [out] *pY                  : Pointer to the output matrix.
    *                                     Initializes @ref _pY.
    * @param [in]  widthIn              : Width of input matrix.
    * @param [in]  heightIn             : Height of input matrix.
    * @param [in]  gWidthIn             : Width of input gain matrix.
    * @param [in]  inPitchBytes         : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes        : Pitch of output matrix in bytes.
    * @param [in]  isInterleave         : Indicates whether the input matrix data is interleaved:
    *                                   - `0`: Non-interleaved (Rows represent channels, columns represent samples).
    *                                   - `1`: Interleaved (Rows represent samples, columns represent channels).
    * @param [in]  balance              : Left/Right control.
    *                                     Range: -1.0 (full left) to 1.0 (full right).
    * @param [in]  fade                 : Front/Rear control.
    *                                     Range: -1.0 (full rear) to 1.0 (full front)
    * @param [in]  channelConfig        : Pointer to an integer array defining the speaker type for each channel.
    *                                   - '0': FRONT_LEFT.
    *                                   - '1': FRONT_LEFT_TREBBLE.
    *                                   - '2': FRONT_LEFT_MID.
    *                                   - '3': FRONT_LEFT_WOOFER.
    *                                   - '4': FRONT_RIGHT.
    *                                   - '5': FRONT_RIGHT_TREBBLE.
    *                                   - '6': FRONT_RIGHT_MID.
    *                                   - '7': FRONT_RIGHT_WOOFER.
    *                                   - '8': FRONT_CENTER.
    *                                   - '9': LOW_FREQUENCY_EFFECTS.
    *                                   - '10': SIDE_LEFT.
    *                                   - '11': SIDE_RIGHT.
    *                                   - '12': REAR_LEFT.
    *                                   - '13': REAR_RIGHT.
    *                                   - '14': TOP_FRONT_LEFT.
    *                                   - '15': TOP_FRONT_RIGHT.
    *                                   - '16': TOP_MIDDLE_LEFT.
    *                                   - '17': TOP_MIDDLE_RIGHT.
    *                                   - '18': TOP_REAR_LEFT.
    *                                   - '19': TOP_REAR_RIGHT.
    *                                   - '20': OVERHEAD.
    * @param [in]  lfeBalanceGainFactor : LFE balance gain factor.
    *                                     Range: 0.0 (no effect) to 1.0 (full effect).
    * @param [in]  lfeFaderGainFactor   : LFE fader gain factor.
    *                                     Range: 0.0 (no effect) to 1.0 (full effect).
    * @param [in]  sideGainFactor       : Side gain factor. Range: 0.0 to 1.0.
    * @param [in]  lfeBalanceMode       : The selected mode for how the LFE channel responds to the balance control.
    *                                   - '0': LFE_BALANCE_UNAFFECTED.
    *                                   - '1': LFE_BALANCE_PARTIAL.
    *                                   - '2': LFE_BALANCE_FULL.
    * @param [in]  lfeFaderMode         : The selected mode for how the LFE channel responds to the fader control.
    *                                   - '0': LFE_FADER_UNAFFECTED.
    *                                   - '1': LFE_FADER_AVERAGE.
    *                                   - '2': LFE_FADER_POSITION_BASED.
    *                                   - '3': LFE_FADER_FULL.
    * @param [in]  errorCtx             : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   BalanceFader(T         *pX,
                T         *pG,
                T         *pY,
                size_t     widthIn,
                size_t     heightIn,
                size_t     gWidthIn,
                size_t     inPitchBytes,
                size_t     outPitchBytes,
                size_t     isInterleave,
                float      balance,
                float      fade,
                int32_t   *channelConfig,
                float      lfeBalanceGainFactor,
                float      lfeFaderGainFactor,
                float      sideGainFactor,
                int32_t    lfeBalanceMode,
                int32_t    lfeFaderMode,
                ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_balanceFader node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param [in]  *pX                  : Pointer to the input matrix
    * @param [in]  *pG                  : Pointer to the input gain values array
    * @param [out] *pY                  : Pointer to the output matrix
    * @param [in]  widthIn              : Width of input matrix
    * @param [in]  heightIn             : Height of input matrix
    * @param [in]  gWidthIn             : Width of input gain matrix
    * @param [in]  inPitchBytes         : Pitch of input matrix in bytes
    * @param [in]  outPitchBytes        : Pitch of output matrix in bytes
    * @param [in]  isInterleave         : Indicates whether input data is interleaved
    * @param [in]  balance              : Left/Right control
    * @param [in]  fade                 : Front/Rear control
    * @param [in]  channelConfig        : Pointer to channel configuration array
    * @param [in]  lfeBalanceGainFactor : LFE balance gain factor
    * @param [in]  lfeFaderGainFactor   : LFE fader gain factor
    * @param [in]  sideGainFactor       : Side gain factor
    * @param [in]  lfeBalanceMode       : LFE balance mode
    * @param [in]  lfeFaderMode         : LFE fader mode
    * @param [in]  nodeName             : Name of the node; sets @ref _nodeInfo name.
    * @param [in]  nodeId               : ID of the node; sets @ref _nodeInfo ID.
    * @param [in]  errorCtx             : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   BalanceFader(T          *pX,
                T          *pG,
                T          *pY,
                size_t      widthIn,
                size_t      heightIn,
                size_t      gWidthIn,
                size_t      inPitchBytes,
                size_t      outPitchBytes,
                size_t      isInterleave,
                float       balance,
                float       fade,
                int32_t    *channelConfig,
                float       lfeBalanceGainFactor,
                float       lfeFaderGainFactor,
                float       sideGainFactor,
                int32_t     lfeBalanceMode,
                int32_t     lfeFaderMode,
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
    * @throw None
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
    * @throw None
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _pX)
    * @throw None
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pY)
    * @throw None
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param [in] addrs  : Structure containing source and destination addresses
    * @param [in] mode   : Mode of address setting (input, output, or both)
    * @retval None
    * @throw None
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute the operation
    *
    * This function performs the actual operation.
    *
    * @retval None
    * @throw None
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
    * @throw None
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
    * @throw None
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

   /**
    * @brief Get the name of the node
    *
    * This function returns the name of the node.
    *
    * @retval const char* The name of the node
    * @throw None
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node
    *
    * This function returns the ID of the node.
    *
    * @retval uint32_t The ID of the node
    * @throw None
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node
    *
    * This function returns a string containing the parameters of the node.
    *
    * @retval const char* The parameters of the node
    * @throw None
    */
   const char *getNodeParams() const noexcept override { return _nodeInfo.getNodeParams(); }

   /**
    * @brief Get the input and output buffer parameters of the node
    *
    * This function returns reference to node's i/o buffer properties
    *
    * @retval const NodeBufParams& : A constant reference to the NodeBufParams representing node's i/o buffer properties
    * @throw None
    */
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    * @retval None
    * @throw None
    */

   ~BalanceFader() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
