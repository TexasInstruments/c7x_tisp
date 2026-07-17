// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"

namespace TISP {
namespace MATHLIB {

/**
 *  @ingroup mathlibNode
 */

/**@{*/

/**
 * \class Div
 * @brief Provides node-level APIs for the MATHLIB_div kernel from MATHLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel computes elementwise divide of dividend(pIn0) and corresponding divisor(pIn1)
 * and writes the resulting values to an output array pOut.
 */

template <typename T> class Div final : public VirtualOp {

   size_t   _size;     /*!< Number of elements */
   T       *_pIn0;     /*!< Pointer to input array 0. */
   T       *_pIn1;     /*!< Pointer to input array 1. */
   T       *_pOut;     /*!< Pointer to output array. */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

 public:
   /**
    * @brief Creates a MATHLIB_div node.
    *
    * This constructor initializes a node for the MATHLIB_div kernel.
    *
    * @param [in]  *pIn0  : Pointer to the input array 0.
    *                      Initializes @ref _pIn0.
    * @param [in]  *pIn1  : Pointer to the input array 1.
    *                      Initializes @ref _pIn1.
    * @param [out] *pOut : Pointer to the output array.
    *                      Initializes @ref _pOut.
    * @param [in]  size  : Length of input array.
    *                      Initializes _size.
    * @param [in]  errorCtx : Error context for reporting constructor errors
    *
    */
   Div(size_t size, T *pIn0, T *pIn1, T *pOut, ErrorCtxt &errorCtx);

   /**
    * @brief Creates a MATHLIB_div node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param [in]  size     : Length of input array.
    * @param [in]  *pIn0    : Pointer to the input array 0.
    * @param [in]  *pIn1    : Pointer to the input array 1.
    * @param [out] *pOut    : Pointer to the output array.
    * @param [in]  nodeName : Name of the node; initializes @ref _nodeInfo.
    * @param [in]  nodeId   : ID of the node; initializes @ref _nodeInfo.
    * @param [in]  errorCtx : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   Div(size_t size, T *pIn0, T *pIn1, T *pOut, const char *nodeName, uint32_t nodeId, ErrorCtxt &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pIn0 (input array)
    * - index 1: _pIn1 (input array)
    *
    * @param[in]  index : Index of the input buffer (0 or 1)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    * @throw None
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
    * @throw None
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (2: _pIn0 and _pIn1)
    * @throw None
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pOut)
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

   ~Div() = default;
};

/** @} */
} // namespace MATHLIB
} // namespace TISP
