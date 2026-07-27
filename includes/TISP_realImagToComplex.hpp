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
 * \class RealimagToComplex
 * @brief Provides node-level APIs for the DSPLIB_realImagToComplex kernel from DSPLIB.
 * @tparam T : Supports float, double, int16_t, uint16_t, int32_t, uint32_t.
 *
 * @details This kernel takes separate real and imaginary matrices of size NxM and produces
 * an output matrix of size Nx(2*M), where the columns are interleaved as:
 * real_0, imag_0, real_1, imag_1, ..., real_(M-1), imag_(M-1).
 *
 * @par Matrix Representation:
 *
 * Input Real Matrix (NxM):
 * \f[
 * \text{Real} =
 * \begin{bmatrix}
 * r_{0,0} & r_{0,1} & \cdots & r_{0,M-1} \\
 * r_{1,0} & r_{1,1} & \cdots & r_{1,M-1} \\
 * \vdots & \vdots & \ddots & \vdots \\
 * r_{N-1,0} & r_{N-1,1} & \cdots & r_{N-1,M-1} \\
 * \end{bmatrix}
 * \f]
 *
 * Input Imaginary Matrix (NxM):
 * \f[
 * \text{Imag} =
 * \begin{bmatrix}
 * i_{0,0} & i_{0,1} & \cdots & i_{0,M-1} \\
 * i_{1,0} & i_{1,1} & \cdots & i_{1,M-1} \\
 * \vdots & \vdots & \ddots & \vdots \\
 * i_{N-1,0} & i_{N-1,1} & \cdots & i_{N-1,M-1} \\
 * \end{bmatrix}
 * \f]
 *
 * Output Matrix (Nx2M):
 * \f[
 * \text{Output} =
 * \begin{bmatrix}
 * r_{0,0} & i_{0,0} & r_{0,1} & i_{0,1} & \cdots & r_{0,M-1} & i_{0,M-1} \\
 * r_{1,0} & i_{1,0} & r_{1,1} & i_{1,1} & \cdots & r_{1,M-1} & i_{1,M-1} \\
 * \vdots & \vdots & \vdots & \vdots & \ddots & \vdots & \vdots \\
 * r_{N-1,0} & i_{N-1,0} & r_{N-1,1} & i_{N-1,1} & \cdots & r_{N-1,M-1} & i_{N-1,M-1} \\
 * \end{bmatrix}
 * \f]
 *
 * @note The input matrices `real` and `imag` must have the same dimensions.
 */

template <typename T> class RealimagToComplex final : public VirtualOp {

   T *_pIn0; /*!< Pointer to input array 0. */
   T *_pIn1; /*!< Pointer to input array 1. */
   T *_pOut; /*!< Pointer to output array. */

   size_t _widthIn;
   size_t _heightIn;
   size_t _widthOut;
   size_t _heightOut;
   size_t _inPitchBytes;
   size_t _outPitchBytes;

   TISP::HandlePtr<char> _handle; /*!< handle required by DSPLIB_realImagToComplex kernel */

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

 public:
   /**
    * @brief Creates a DSPLIB_RealimagToComplex node.
    *
    * This constructor initializes a node for the DSPLIB_RealimagToComplex kernel.
    *
    * @param [in]  *pIn0         : Pointer to the input array 0.
    *                              Initializes @ref _pIn0.
    * @param [in]  *pIn1         : Pointer to the input array 1.
    *                              Initializes @ref _pIn1.
    * @param [out] *pOut         : Pointer to the output array.
    *                              Initializes @ref _pOut.
    * @param [in]  widthIn       : Width of input matrix.
    * @param [in]  heightIn      : Height of input matrix.
    * @param [in]  widthOut      : Width of output matrix.
    * @param [in]  heightOut     : Height of output matrix.
    * @param [in]  inPitchBytes  : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes : Pitch of output matrix in bytes.
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    *
    */
   RealimagToComplex(T         *pIn0,
                     T         *pIn1,
                     T         *pOut,
                     size_t     widthIn,
                     size_t     heightIn,
                     size_t     widthOut,
                     size_t     heightOut,
                     size_t     inPitchBytes,
                     size_t     outPitchBytes,
                     ErrorCtxt &errorCtx);

   /**
    * @brief Creates a DSPLIB_RealimagToComplex node with node name and ID.
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   RealimagToComplex(T          *pIn0,
                     T          *pIn1,
                     T          *pOut,
                     size_t      widthIn,
                     size_t      heightIn,
                     size_t      widthOut,
                     size_t      heightOut,
                     size_t      inPitchBytes,
                     size_t      outPitchBytes,
                     const char *nodeName,
                     uint32_t    nodeId,
                     ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pIn0 (input data matrix 0) - managed by setAddr()
    * - index 1: _pIn1 (input data matrix 1) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0 or 1)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pOut (output data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (2: _pIn0 and _pIn1)
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
    * This function sets the source and destination addresses for the operation
    *
    * @param [in] addrs  : Structure containing source and destination addresses
    * @param [in] mode   : Mode of address setting (input, output, or both)
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
    * @brief Get the node name
    *
    * @return Pointer to the node name string
    * @retval const char*
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the node ID
    *
    * @return Node ID
    * @retval uint32_t
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the node parameters
    *
    * @return Pointer to the parameters string
    * @retval const char*
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
    * This destructor is set to default
    *
    */

   ~RealimagToComplex() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
