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
 * \class AddNch
 * @brief Provides node-level APIs for the DSPLIB_addNCh kernel from DSPLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel processes multiple input matrices, arranged as a single 3D matrix pX, where each matrix has
 * dimensions NxM and there are C such matrices. It produces an output matrix pY of size NxM, where each element is the
 * sum of the corresponding values from all input channels.
 *
 * @par Matrix Representation:
 *
 * Input Matrix (2 inputs stacked, size CxNxM):
 * \f[
 * X =
 * \begin{bmatrix}
 * 1 & 2 & \cdots & 10 \\
 * 2 & 3 & \cdots & 11 \\
 * \vdots & \vdots & \ddots & \vdots \\
 * 10 & 11 & \cdots & 19 \\
 * \hline
 * 1 & 2 & \cdots & 10 \\
 * 2 & 3 & \cdots & 11 \\
 * \vdots & \vdots & \ddots & \vdots \\
 * 10 & 11 & \cdots & 19 \\
 * \end{bmatrix}
 * \f]
 *
 * Output Matrix (summation across inputs, size NxM):
 * \f[
 * Y =
 * \begin{bmatrix}
 * 2 & 4 & \cdots & 20 \\
 * 4 & 6 & \cdots & 22 \\
 * \vdots & \vdots & \ddots & \vdots \\
 * 20 & 22 & \cdots & 38 \\
 * \end{bmatrix}
 * \f]
 *
 */

template <typename T> class AddNch final : public VirtualOp {

   T *_pX; /*!< Pointer to input matrix. */
   T *_pY; /*!< Pointer to output matrix. */

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level DSPLIB_addNCh kernel. */

 public:
   /**
    * @brief Creates an DSPLIB_addNCh node.
    *
    * This constructor initializes a node for the DSPLIB_addNCh kernel.
    *
    * @param [in]  *pX            : Pointer to the input matrix.
    *                               Initializes @ref _pX.
    * @param [out] *pY            : Pointer to the output matrix.
    *                               Initializes @ref _pY.
    * @param [in]  width          : Width of input matrix.
    * @param [in]  height         : Height of input matrix.
    * @param [in]  inputs         : Length of input matrix in Z dimension.
    * @param [in]  inPitchBytesY  : Y dimension Pitch of input matrix in bytes.
    * @param [in]  inPitchBytesZ  : Z dimension Pitch of input matrix in bytes.
    * @param [in]  errorCtx       : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   AddNch(T         *pX,
          T         *pY,
          size_t     width,
          size_t     height,
          size_t     inputs,
          size_t     inPitchBytesY,
          size_t     inPitchBytesZ,
          ErrorCtxt &errorCtx);

   /**
    * @brief Creates an DSPLIB_addNCh node with a name and ID.
    *
    * This constructor is a delegator that accepts a node name and ID.
    *
    * @param [in]  *pX            : Pointer to the input matrix.
    *                               Initializes @ref _pX.
    * @param [out] *pY            : Pointer to the output matrix.
    *                               Initializes @ref _pY.
    * @param [in]  width          : Width of input matrix.
    * @param [in]  height         : Height of input matrix.
    * @param [in]  inputs         : Length of input matrix in Z dimension.
    * @param [in]  inPitchBytesY  : Y dimension Pitch of input matrix in bytes.
    * @param [in]  inPitchBytesZ  : Z dimension Pitch of input matrix in bytes.
    * @param [in]  nodeName       : Name of the node; initializes @ref _nodeInfo
    * @param [in]  nodeId         : ID of the node; initializes @ref _nodeInfo
    * @param [in]  errorCtx       : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   AddNch(T          *pX,
          T          *pY,
          size_t      width,
          size_t      height,
          size_t      inputs,
          size_t      inPitchBytesY,
          size_t      inPitchBytesZ,
          const char *nodeName,
          uint32_t    nodeId,
          ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pX (input array)
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
    * - index 0: _pY (output array)
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
    * @brief Execute the operation
    *
    * This function performs the actual operation.
    *
    * @retval None
    * @throw None
    */

   void exec() noexcept override;

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    * @retval None
    * @throw None
    */

   ~AddNch() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
