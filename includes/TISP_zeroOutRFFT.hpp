// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_common.hpp"
#include <cstddef>
#include <cstdint>

namespace TISP {

namespace misc {

/**
 *  @ingroup miscNode
 */

/**@{*/

/**
 *  \class zeroOutRFFT
 *  @brief Provides node-level APIs for the block copy kernel
 *  @tparam T        : Supports float
 */

template <typename T> class zeroOutRFFT final : public VirtualOp {

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *pX; /*!< Pointer to address of input vector */

   size_t numPoints;   /*!< width of input block */
   size_t numChannels; /*!< height of input block */

   size_t   index;
   int32_t *length;

   __SE_TEMPLATE_v1 se0Param = __gen_SE_TEMPLATE_v1(); /*!< structure to hold SE params*/
   __SA_TEMPLATE_v1 sa0Param = __gen_SA_TEMPLATE_v1(); /*!< structure to hold SA params*/

   typedef typename c7x::make_full_vector<T>::type vec;

   /**
    * @brief Optimized function that uses SE/SA to zero out RFFT bins; this function is not exposed to user
    * @param [in]  _pX          : Pointer to input/output 2D block (zeroed in place)
    * @param [in]  _numPoints   : Width of 2D block
    * @param [in]  _numChannels : Height of 2D block
    * @param [in]  _index       : Start index from which to zero out
    * @param [in]  _length      : Number of elements to zero out
    */

   void exec_ci(T *_pX, size_t _numPoints, size_t _numChannels, size_t _index, size_t _length);

 public:
   /**
    * @brief Get the name of the node
    *
    * @retval const char* The name of the node
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node
    *
    * @retval uint32_t The ID of the node
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node
    *
    * @retval const char* The parameters of the node
    */
   const char *getNodeParams() const noexcept override { return _nodeInfo.getNodeParams(); }

   /**
    * @brief Get the input and output buffer parameters of the node
    *
    * @retval const NodeBufParams& : A constant reference to the NodeBufParams representing node's i/o buffer properties
    */
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Creates a zeroOutRFFT node
    * @param [in]  _pX         : Pointer to input 2D block; initializes @ref pX
    * @param [in]  _numPoints  : Width of input 2D block; initializes @ref numPoints
    * @param [in]  _numChannels: Height of input 2D block; initializes @ref numChannels
    * @param [in]  _index      : Start index from which to zero out; initializes @ref index
    * @param [in]  _length     : Pointer to per-channel zero-out length; initializes @ref length
    *
    */

   zeroOutRFFT(T *_pX, size_t _numPoints, size_t _numChannels, size_t _index, int32_t *_length);

   /**
    * @brief Set source and destination addresses for the operation
    *
    * @param[in] addrs Structure containing source and destination addresses
    * @param[in] mode Mode of address setting (input, output, or both)
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute the operation
    *
    */
   void exec() noexcept override;

   ~zeroOutRFFT() {}
};

/** @} */

} // namespace misc

} // namespace TISP
