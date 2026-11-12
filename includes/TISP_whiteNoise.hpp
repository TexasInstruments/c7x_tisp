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
 * @brief Construction-time (non-tunable) parameters of the @ref Whitenoise node.
 *
 * @details Required; sets the output range and the generator seed.
 */
struct WhitenoiseConfig {
   float    range = 1.0f; /*!< The output range; must be greater than zero. Noise will be in [-range, +range]. */
   uint32_t seed  = 1U;   /*!< The initial 32-bit seed for the LCG. Any value, including 0, is valid. */
};

/**
 * \class Whitenoise
 * @brief Provides node-level APIs for the AUDIOLIB_whiteNoise kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel creates a frame of pseudo-random white noise by using a 32-bit Linear Congruential Generator
 * (LCG). The LCG generates the next random value based on the current value and three constants a, c and m.
 *
 * The values used by the kernel are:
 * - \f$ a = 1664525 \f$
 * - \f$ c = 1013904223 \f$
 * - \f$ m = 2^{32} \f$
 *
 * Each 32-bit value \f$ x \f$ is centred on zero and scaled to the output range:
 * \f$ y = \frac{x - 2^{31}}{2^{31}} \cdot range \f$, in [-range, +range].
 *
 * The node has no inputs. The output is non-interleaved, one row per channel, at most
 * AUDIOLIB_WHITENOISEGENERATOR_MAX_NUM_CHANNELS channels. Each channel runs its own LCG sequence, all derived from the
 * one seed: channel 0 gives the single-channel output and the channels are mutually uncorrelated.
 *
 * @image html whiteNoise_node_multichannel.svg "Whitenoise node: multichannel noise from one seed" width=900px
 **/

template <typename T> class Whitenoise final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_whiteNoise kernel */

 public:
   /**
    * @brief Creates an AUDIOLIB_whiteNoise node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_whiteNoise kernel for @c height
    *          channels of @c width samples.
    *
    * @param [in] pInputs  : Input buffer descriptors; none: `nullptr` or an empty group `{ nullptr, 0 }`.
    * @param [in] pOutputs : Output buffer descriptors; exactly one non-interleaved buffer, one row per channel.
    *                        Its @c pBuf is the initial output pointer, rebound later by @ref setAddr.
    * @param [in] pConfig  : Construction-time parameters; required.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx : Error context for reporting constructor errors
    *
    * @throw None
    */
   Whitenoise(const TISP_BufDescList *pInputs,
              const TISP_BufDescList *pOutputs,
              const WhitenoiseConfig *pConfig,
              const char             *nodeName,
              uint32_t                nodeId,
              ErrorCtxt              &errorCtx);

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
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: output buffer)
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set the destination address for the operation
    *
    * This function sets the output (destination) address for the operation.
    * This kernel has no inputs; only the output buffer address is relevant.
    *
    * @param [in] addrs : Structure containing the destination address
    * @param [in] mode  : Mode of address setting (output)
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
   ~Whitenoise() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
