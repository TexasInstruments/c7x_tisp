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
 *  \class Interleave
 *  @brief Provides node-level APIs for the DSPLIB_interleave kernel from DSPLIB.
 *  @tparam T        :  Supports float, double, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t
 */

template <typename T> class Interleave final : public VirtualOp {
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pX; /*!< Pointer to address of input vector */
   T *_pY; /*!< Pointer to address of result vector*/

   size_t _width;
   size_t _height;
   size_t _inPitchBytes;
   size_t _outPitchBytes;

   TISP::HandlePtr<char> _handle; /*!< handle required by DSPLIB_interleave kernel */

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
    * @brief Creates a DSPLIB_Interleave node
    *
    * This constructor is called to create the node
    *
    * @param[in]  pX       : Pointer to input matrix; initializes @ref _pX
    * @param[out] pY       : Pointer to output matrix; initializes @ref _pY
    * @param[in]  width    : Width of input matrix
    * @param[in]  height   : Height of input matrix
    * @param[in]  inPitchBytes  : Pitch of input matrix in bytes
    * @param[in]  outPitchBytes : Pitch of output matrix in bytes
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    *
    */
   Interleave(T         *pX,
              T         *pY,
              size_t     width,
              size_t     height,
              size_t     inPitchBytes,
              size_t     outPitchBytes,
              ErrorCtxt &errorCtx);

   /**
    * @brief Creates a Interleave node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Interleave(T          *pX,
              T          *pY,
              size_t      width,
              size_t      height,
              size_t      inPitchBytes,
              size_t      outPitchBytes,
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

   ~Interleave() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
