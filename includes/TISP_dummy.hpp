// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_common.hpp"

namespace TISP {

namespace misc {

/**
 *  @ingroup miscNode
 */

/**@{*/

/**
 *  \class Dummy
 *  @brief Provides node-level APIs for the dummy kernel (does nothing)
 *  @tparam T        : Supports float
 */

template <typename T> class Dummy final : public VirtualOp {

   T *_pX; /*!< Pointer to address of input vector */
   T *_pY; /*!< Pointer to address of result vector*/

   size_t _width;  /*!< width of input block */
   size_t _height; /*!< height of input block */

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   __SE_TEMPLATE_v1 _se0Param = __gen_SE_TEMPLATE_v1(); /*!< structure to hold SE params*/
   __SA_TEMPLATE_v1 _sa0Param = __gen_SA_TEMPLATE_v1(); /*!< structure to hold SA params*/

   using vec = typename c7x::make_full_vector<T>::type;

   /**
    * @brief Optimized function that uses SE/SA to perform block copy; this function is not exposed to user
    * @param [in]  *pX    : Pointer to input 2D block
    * @param [out] *pY    : Pointer to output 2D block
    * @param [in]  width  : Width of input 2D block
    * @param [in]  height : Height of input 2D block
    * @retval None
    * @throw None
    */

   void exec_ci(T *pX, T *pY, size_t width, size_t height) noexcept;

 public:
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
    * @brief Creates a Dummy node
    *
    * This constructor is called to create the node
    *
    * @param [in]  *pX    : Pointer to input 2D block; initializes @ref _pX
    * @param [out] *pY    : Pointer to output 2D block; initializes @ref _pY
    * @param [in]  width  : Width of input 2D block; initializes @ref _width
    * @param [in]  height : Height of input 2D block; initializes @ref _height
    */

   Dummy(T *pX, T *pY, size_t width, size_t height);

   /**
    * @brief Creates a Dummy node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param[in] T *pX, T *pY, size_t width, size_t height
    * @param[in] nodeName          : Name of the node; initializes @ref _nodeInfo
    * @param[in] nodeId            : ID of the node; initializes @ref _nodeInfo
    * @retval None
    * @throw None
    */
   Dummy(T *pX, T *pY, size_t width, size_t height, const char *nodeName, uint32_t nodeId);

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

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    * @retval None
    * @throw None
    */

   ~Dummy() = default;
};

/** @} */

} // namespace misc

} // namespace TISP
