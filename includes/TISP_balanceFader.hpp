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
 * @details Applies a per-channel gain derived from balance, fade, and each channel's speaker type
 *          (@ref BalanceFaderTunableParams) to an interleaved or non-interleaved input.
 *
 * <b>Runtime Control Philosophy:</b>
 * The BalanceFader node supports dynamic parameter updates via a robust runtime API. Users can query
 * or modify specific algorithmic parameters (such as balance, fade, LFE settings, and side gain)
 * without needing to reconstruct the node.
 *
 */

enum class BalanceFaderParamId : uint16_t {
   kBypass               = 0, /**< kBypass: uint32_t bypass mode; `0` applies the gain, `1` copies input to output */
   kBalance              = 1, /**< kBalance: Left/Right balance control */
   kFade                 = 2, /**< kFade: Front/Rear fader control */
   kLfeBalanceGainFactor = 3, /**< kLfeBalanceGainFactor: LFE balance gain factor */
   kLfeFaderGainFactor   = 4, /**< kLfeFaderGainFactor: LFE fader gain factor */
   kSideGainFactor       = 5, /**< kSideGainFactor: Side gain factor */
   kLfeBalanceMode       = 6, /**< kLfeBalanceMode: LFE balance mode */
   kLfeFaderMode         = 7, /**< kLfeFaderMode: LFE fader mode */
   kChannelConfig        = 8, /**< kChannelConfig: Speaker type per channel array */
   kAll                  = 9  /**< kAll: all tunable params as one @ref BalanceFaderTunableParams payload */
};

/**
 * @brief Tunable parameters of the @ref BalanceFader node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref BalanceFaderParamId::kAll payload of
 *          @ref BalanceFader::setParams / @ref BalanceFader::getParams.
 *
 * @tparam T : Node element type (float).
 */
template <typename T> struct BalanceFaderTunableParams {
   float    balance              = 0.0F;    /*!< Left/Right control, range [-1, 1]. */
   float    fade                 = 0.0F;    /*!< Front/Rear control, range [-1, 1]. */
   int32_t *pChannelConfig       = nullptr; /*!< Caller-owned array, one speaker type per channel; required, must
                                                 outlive the node. */
   float    lfeBalanceGainFactor = 0.0F;    /*!< LFE balance gain factor, range [0, 1]. */
   float    lfeFaderGainFactor   = 0.0F;    /*!< LFE fader gain factor, range [0, 1]. */
   float    sideGainFactor       = 0.0F;    /*!< Side gain factor, range [0, 1]. */
   int32_t  lfeBalanceMode       = 0;       /*!< LFE response mode to balance control, range [0, 2]. */
   int32_t  lfeFaderMode         = 0;       /*!< LFE response mode to fader control, range [0, 3]. */
   uint32_t bypassMode           = 0U;      /*!< `0`: apply the gain, `1`: copy input to output. */
};

template <typename T> class BalanceFader final : public VirtualOp {

   NodeInfo                     _nodeInfo;    /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D               _in;          /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D               _out;         /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   T                           *_gains;       /*!< Per-channel gain buffer (state), written by the kernel set */
   size_t                       _numChannels; /*!< Number of channels (input width if interleaved, else height) */
   BalanceFaderTunableParams<T> _params;      /*!< Current tunable params; pChannelConfig is the caller's array */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_balanceFader kernel. */

   /**
    * @brief Copies the input buffer described by @ref _in to the output buffer described by @ref _out,
    *        row by row, honouring both row pitches.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

   /**
    * @brief Range-checks a set of tunable parameters.
    *
    * @param [in]     params   : Tunable parameters to check.
    * @param [in]     funcName : Caller identification used in the error message.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true  : All parameters are in range.
    * @retval false : A parameter is out of range or the channel config pointer is null.
    * @throw None
    */
   static bool checkTunableParams(const BalanceFaderTunableParams<T> &params,
                                  const char                         *funcName,
                                  ErrorCtxt                          &errorCtx) noexcept;

   /**
    * @brief Recomputes the per-channel gains from @c params and, on success, makes them the current params.
    *
    * @param [in]     params   : Tunable parameters to apply; @c pChannelConfig must be the bound array.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval None
    * @throw None
    */
   void applyTunableParams(const BalanceFaderTunableParams<T> &params, ErrorCtxt &errorCtx) noexcept;

 public:
   /**
    * @brief Creates an AUDIOLIB_balanceFader node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_balanceFader kernel. The channel
    *          count is the input @c width when interleaved, otherwise its @c height.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one buffer. Its @c pBuf is the
    *                                    initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one buffer with the same layout as
    *                                    the input.
    * @param [in] pStates               : State buffer descriptors; exactly one per-channel gain buffer with a width
    *                                    of one value per channel.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with a non-null @c pChannelConfig.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   BalanceFader(const TISP_BufDescList             *pInputs,
                const TISP_BufDescList             *pOutputs,
                const TISP_BufDescList             *pStates,
                const BalanceFaderTunableParams<T> *pDefaultTunableParams,
                const char                         *nodeName,
                uint32_t                            nodeId,
                ErrorCtxt                          &errorCtx);

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
    * This destructor is set to default.
    */
   ~BalanceFader() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
