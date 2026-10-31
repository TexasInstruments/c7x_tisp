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
 *  @tparam dTypeIn  : Input data type
 *  @tparam dTypeOut : Output data type
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
    * @param [in]  pX     : Pointer to input 2D block
    * @param [out] pY     : Pointer to output 2D block
    */

   void exec_ci(dTypeIn *pX, dTypeOut *pY);

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
    * @brief Creates a DataConvert node
    * @param [in]  pX             : Pointer to input 2D block; initializes @ref _pX
    * @param [out] pY             : Pointer to output 2D block; initializes @ref _pY
    * @param [in]  width          : Width of input 2D block; initializes @ref _width
    * @param [in]  height         : Height of input 2D block; initializes @ref _height
    * @param [in]  strideInBytes  : Stride of input block in bytes
    * @param [in]  strideOutBytes : Stride of output block in bytes
    * @param [in]  errorCtx       : Error context for reporting constructor errors
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
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
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
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _pX)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pY)
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

   ~DataConvert() = default;
};

/** @} */
} // namespace misc

} // namespace TISP

#endif
