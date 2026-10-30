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
 * @brief Construction-time (non-tunable) parameters of the @ref PreEmphasis node.
 *
 * @details Required; sets the filter coefficient.
 */
struct PreEmphasisConfig {
   float alpha = 0.97f; /*!< Pre-emphasis coefficient, range [0, 1]. */
};

/**
 * \class PreEmphasis
 * @brief Provides node-level APIs for the AUDIOLIB_preEmphasis kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details Applies the pre-emphasis filter y[n] = x[n] - alpha * x[n-1] to every channel and writes the output;
 *          the filter state carries across frames. Input and output share one layout (interleaved or
 *          non-interleaved), hold at most 64 channels and must be different buffers.
 **/

template <typename T> class PreEmphasis final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_preEmphasis kernel */

 public:
   /**
    * @brief Creates an AUDIOLIB_preEmphasis node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_preEmphasis kernel. The channel
    *          count is the input @c width when interleaved, otherwise its @c height.
    *
    * @param [in] pInputs      : Input buffer descriptors; exactly one buffer. Its @c pBuf is the
    *                            initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs     : Output buffer descriptors; exactly one buffer with the same layout as
    *                            the input.
    * @param [in] pConfig : Construction-time parameters; required.
    * @param [in] nodeName     : Name of the node
    * @param [in] nodeId       : ID of the node
    * @param [in] errorCtx     : Error context for reporting constructor errors
    *
    * @throw None
    */
   PreEmphasis(const TISP_BufDescList  *pInputs,
               const TISP_BufDescList  *pOutputs,
               const PreEmphasisConfig *pConfig,
               const char              *nodeName,
               uint32_t                 nodeId,
               ErrorCtxt               &errorCtx);

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
   ~PreEmphasis() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
