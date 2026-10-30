// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <array>

namespace TISP {

namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 * \class Split
 * @brief Provides node-level APIs for the AUDIOLIB_split kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details Splits the channels of one input into multiple outputs, each with its own channel count.
 *          Input and outputs share one layout (interleaved or non-interleaved).
 */

template <typename T> class Split final : public VirtualOp {

   NodeInfo                        _nodeInfo;   /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D                  _in;         /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   std::array<void *, kMaxDstAddr> _outputs;    /*!< Output buffer pointers; @ref setAddr rebinds them */
   size_t                          _numOutputs; /*!< Number of output buffers */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_split kernel */

 public:
   /**
    * @brief Creates an AUDIOLIB_split node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_split kernel. Each output's
    *          channel count is its @c width when interleaved, otherwise its @c height.
    *
    * @param [in] pInputs  : Input buffer descriptors; exactly one buffer holding all output channels.
    * @param [in] pOutputs : Output buffer descriptors; 1 to @ref kMaxDstAddr buffers, with the same
    *                       layout as the input.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx : Error context for reporting constructor errors
    *
    * @throw None
    */
   Split(const TISP_BufDescList *pInputs,
         const TISP_BufDescList *pOutputs,
         const char             *nodeName,
         uint32_t                nodeId,
         ErrorCtxt              &errorCtx);

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
    * - index 0 to _numOutputs-1: output buffers (@ref _outputs) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0 to _numOutputs-1)
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
    * @retval    size_t : The number of output buffers (_numOutputs)
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param [in] addrs  : Structure containing source and destination addresses.
    * @param [in] mode   : Mode of address setting (input, output, or both).
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute the operation
    *
    * This function performs the actual operation.
    */
   void exec() noexcept override;

   /**
    * @brief Get the name of the node
    *
    * This function returns the name of the node.
    *
    * @retval const char* The name of the node.
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node
    *
    * This function returns the ID of the node.
    *
    * @retval uint32_t The ID of the node.
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node
    *
    * This function returns a string containing the parameters of the node.
    *
    * @retval const char* The parameters of the node.
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
   ~Split() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP

/* ======================================================================== */
/*  End of file:  TISP_split.hpp                                            */
/* ======================================================================== */
