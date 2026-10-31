// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"

namespace TISP {
namespace DSPLIB {

/**
 *  @ingroup dsplibNode
 */

/**@{*/

/**
 *  \class AddConstant
 *  @brief Provides node-level APIs for the DSPLIB_addConstant kernel from DSPLIB.
 *  @tparam T        : Supports float
 */

template <typename T> class AddConstant final : public VirtualOp {
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pIn0; /*!< Pointer to input constant */
   T *_pIn1; /*!< Pointer to address of input vector */
   T *_pOut; /*!< Pointer to address of result vector */

   TISP::HandlePtr<char> _handle; /*!< Handle required by DSPLIB_addConstant kernel */
   size_t                  _size;   /*!< Length of vector */

 public:
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
    * @brief Creates an AddConstant node
    *
    * This constructor is called to create the node
    *
    * @param [in]  pIn0  : Pointer to input constant; initializes @ref _pIn0
    * @param [in]  pIn1  : Pointer to address of input vector; initializes @ref _pIn1
    * @param [out] pOut  : Pointer to address of output vector; initializes @ref _pOut
    * @param [in]  size  : length of vector; initializes _size
    * @param [in]  errorCtx : Error context for reporting constructor errors
    *
    */

   AddConstant(T *pIn0, T *pIn1, T *pOut, size_t size, ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AddConstant node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   AddConstant(T *pIn0, T *pIn1, T *pOut, size_t size, const char *nodeName, uint32_t nodeId, ErrorCtxt &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pIn1 (input array) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pOut (output array)
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _pIn1)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pOut)
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param[in] addrs Structure containing source and destination addresses
    * @param[in] mode Mode of address setting (input, output, or both)
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
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~AddConstant() = default;
};

/** @} */
} // namespace DSPLIB
} // namespace TISP
