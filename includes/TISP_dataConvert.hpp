// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_DATA_CONVERT_HPP
#define TISP_DATA_CONVERT_HPP

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <cstddef>

namespace TISP {

namespace misc {

/**
 *  @ingroup miscNode
 */

/**@{*/

/**
 *  \class DataConvert
 *  @brief Provides node-level APIs for the block copy kernel
 *  @tparam T        : Supports float
 *  @tparam dmaDir   : Supports @ref TISP::NO_DMA, @ref TISP::DMA_IN, @ref TISP::DMA_OUT, and @ref TISP::DMA_IN_OUT
 */

template <typename dTypeIn, typename dTypeOut> class DataConvert final : public VirtualOp {

   using vecIn  = typename c7x::make_full_vector<dTypeIn>::type;
   using vecOut = typename c7x::make_full_vector<dTypeOut>::type;

   dTypeIn  *_pX; /*!< Pointer to address of input vector */
   dTypeOut *_pY; /*!< Pointer to address of result vector*/

   size_t _width;  /*!< width of input block */
   size_t _height; /*!< height of input block */

   size_t _strideInBytes;  /*!< strideIn in bytes */
   size_t _strideOutBytes; /*!< strideOut in bytes */

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   __SE_TEMPLATE_v1 _se0Param = __gen_SE_TEMPLATE_v1(); /*!< structure to hold SE params*/
   __SE_TEMPLATE_v1 _se1Param = __gen_SE_TEMPLATE_v1(); /*!< structure to hold SE params*/

   __SA_TEMPLATE_v1 _sa0Param = __gen_SA_TEMPLATE_v1(); /*!< structure to hold SA params*/

   size_t _computeLoopCount = 0;

   /**
    * @brief Optimized function that uses SE/SA to perform block copy; this function is not exposed to user
    * @param [in]  *_pX    : Pointer to input 2D block
    * @param [out] *_pY    : Pointer to output 2D block
    * @param [in]  _width  : Width of input 2D block
    * @param [in]  _height : Height of input 2D block
    * @return void
    */

   void exec_ci(dTypeIn *pX, dTypeOut *pY);

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
    * @brief Creates a DataConvert node
    * @param [in]  pX      : Pointer to input 2D block; initializes @ref _pX
    * @param [out] pY      : Pointer to output 2D block; initializes @ref _pY
    * @param [in]  width   : Width of input 2D block; initializes @ref _width
    * @param [in]  height  : Height of input 2D block; initializes @ref _height
    * @param [in]  errorCtx : Error context for reporting constructor errors
    */
   DataConvert(dTypeIn   *pX,
               dTypeOut  *pY,
               size_t     width,
               size_t     height,
               size_t     strideInBytes,
               size_t     strideOutBytes,
               ErrorCtxt &errorCtx);

   /**
    * @brief Creates a DataConvert node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param[in]  *pX              : Pointer to input 2D block; initializes @ref _pX
    * @param[out] *pY              : Pointer to output 2D block; initializes @ref _pY
    * @param[in]  width            : Width of input 2D block; initializes @ref _width
    * @param[in]  height           : Height of input 2D block; initializes @ref _height
    * @param[in]  strideInBytes    : Stride of input in bytes; initializes @ref _strideInBytes
    * @param[in]  strideOutBytes   : Stride of output in bytes; initializes @ref _strideOutBytes
    * @param[in]  nodeName         : Name of the node; initializes @ref _nodeInfo
    * @param[in]  nodeId           : ID of the node; initializes @ref _nodeInfo
    * @param [in]  errorCtx        : Error context for reporting constructor errors
    * @retval None
    * @throw None
    */
   DataConvert(dTypeIn    *pX,
               dTypeOut   *pY,
               size_t      width,
               size_t      height,
               size_t      strideInBytes,
               size_t      strideOutBytes,
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

   ~DataConvert() = default;
};

/** @} */
} // namespace misc

} // namespace TISP

#endif
