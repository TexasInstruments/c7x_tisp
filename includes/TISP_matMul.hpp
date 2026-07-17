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
 *  \class matMul
 *  @brief Provides node-level APIs for the DSPLIB_matMul kernel from DSPLIB.
 *  @tparam T        : Supports float and double
 *  @tparam dmaDir   : Supports @ref TISP::NO_DMA, @ref TISP::DMA_IN, @ref TISP::DMA_OUT, and @ref TISP::DMA_IN_OUT
 */

template <typename T> class MatMul final : public VirtualOp {

   T *pA; /*!< Pointer to address of input vector A */
   T *pB; /*!< Pointer to address of input vector B */
   T *pY; /*!< Pointer to address of result vector*/

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   std::unique_ptr<char[]> handle; /*!< handle required by DSPLIB_matMul kernel (stores SE/SA params for the kernel) */

 public:
   /**
    * @brief Creates a DSPLIB_matMul node
    * @param [in]  *_pA      : Pointer to input matrix A; initializes @ref pA
    * @param [in]  *_pB      : Pointer to input matrix B; initializes @ref pB
    * @param [out] *_pY      : Pointer to output matrix; initializes @ref pY
    * @param [in]  _widthA    : Width of input matrix A
    * @param [in]  _heightA   : Height of input matrix A
    * @param [in]  _widthB    : Width of input matrix B
    * @param [in]  _heightB   : Height of input matrix B
    * @param [in]  _inPitchA  : Pitch of input matrix A
    * @param [in]  _inPitchB  : Pitch of input matrix B
    * @param [in]  _outPitch : Pitch of output matrix
    * @param [in]  errorCtx   : Error context for reporting constructor errors
    *
    */
   MatMul(T         *_pA,
          T         *_pB,
          T         *_pY,
          size_t     _widthA,
          size_t     _heightA,
          size_t     _widthB,
          size_t     _heightB,
          size_t     _inPitchA,
          size_t     _inPitchB,
          size_t     _outPitch,
          ErrorCtxt &errorCtx);

   /**
    * @brief Creates a MatMul node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param [in]  *_pA      : Pointer to input matrix A; initializes @ref pA
    * @param [in]  *_pB      : Pointer to input matrix B; initializes @ref pB
    * @param [out] *_pY      : Pointer to output matrix; initializes @ref pY
    * @param [in]  _widthA    : Width of input matrix A
    * @param [in]  _heightA   : Height of input matrix A
    * @param [in]  _widthB    : Width of input matrix B
    * @param [in]  _heightB   : Height of input matrix B
    * @param [in]  _inPitchA  : Pitch of input matrix A
    * @param [in]  _inPitchB  : Pitch of input matrix B
    * @param [in]  _outPitch : Pitch of output matrix
    * @param[in] nodeName          : Name of the node; initializes @ref _nodeInfo
    * @param[in] nodeId            : ID of the node; initializes @ref _nodeInfo
    * @param [in]  errorCtx        : Error context for reporting constructor errors
    *
    * @retval None
    * @throw None
    */
   MatMul(T          *_pA,
          T          *_pB,
          T          *_pY,
          size_t      _widthA,
          size_t      _heightA,
          size_t      _widthB,
          size_t      _heightB,
          size_t      _inPitchA,
          size_t      _inPitchB,
          size_t      _outPitch,
          const char *nodeName,
          uint32_t    nodeId,
          ErrorCtxt  &errorCtx);

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
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pA (input array A)
    * - index 1: _pB (input array B)
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
    * @retval    size_t : The number of input buffers (2: _pA and _pB)
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
    * @param[in] addrs Structure containing source and destination addresses
    * @param[in] mode Mode of address setting (input, output, or both)
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

   ~MatMul() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
