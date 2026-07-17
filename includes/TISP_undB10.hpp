// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"

namespace TISP {

namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 * \class Undb10
 * @brief Provides node-level APIs for the AUDIOLIB_undB10 kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel computes the energy units representation of each sample in the input buffer pX across all
 * channels by dividing sample by 10 and applying a base-10 Exponential function and writes the resulting values to the
 * output buffer pY.
 *
 * @par Method
 * \f[
 * undB10(x) = 10^{(x/10)}
 * \f]
 *
 * <b>Example: </b>
 *
 * <b>Input</b> (pX): No. of samples = N; No. of channels = M.
 *  | Channel/Sample | s0  | s1  | ... | sN-1  |
 *  |----------------|-----|-----|-----|-------|
 *  |      ch0       | 10  | 20  | ... |  30   |
 *  |      ch1       | 30  | 10  | ... |  20   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      chM-1     | 20  | 30  | ... |  10   |
 *
 * <b>Output</b> (pY): No. of samples = N; No. of channels = M.
 *  | Channel/Sample |  s0   |  s1   | ... |  sN-1   |
 *  |----------------|-------|-------|-----|---------|
 *  |      ch0       |   10  |  100  | ... |  1000   |
 *  |      ch1       | 1000  |   10  | ... |   100   |
 *  |      .         |   .   |   .   | ... |    .    |
 *  |      .         |   .   |   .   | ... |    .    |
 *  |      .         |   .   |   .   | ... |    .    |
 *  |      chM-1     |  100  | 1000  | ... |    10   |
 *
 */

template <typename T> class Undb10 final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input matrix. */
   T       *_pY;       /*!< Pointer to output matrix. */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   size_t _width;
   size_t _height;
   size_t _inPitchBytes;
   size_t _outPitchBytes;

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_undB10 kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_undB10 node.
    *
    * This constructor initializes a node for the AUDIOLIB_undB10 kernel.
    *
    * @param [in]  *pX            : Pointer to the input matrix.
    *                               Initializes @ref _pX.
    * @param [out] *pY            : Pointer to the output matrix.
    *                               Initializes @ref _pY.
    * @param [in]  widthIn        : Width of input matrix.
    * @param [in]  heightIn       : Height of input matrix.
    * @param [in]  inPitchBytes   : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes  : Pitch of output matrix in bytes.
    * @param [in]  errorCtx       : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   Undb10(T *pX, T *pY, size_t width, size_t height, size_t inPitchBytes, size_t outPitchBytes, ErrorCtxt &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_undB10 node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param [in]  *pX            : Pointer to the input matrix
    * @param [out] *pY            : Pointer to the output matrix
    * @param [in]  width          : Width of input matrix
    * @param [in]  height         : Height of input matrix
    * @param [in]  inPitchBytes   : Pitch of input matrix in bytes
    * @param [in]  outPitchBytes  : Pitch of output matrix in bytes
    * @param [in]  nodeName       : Name of the node; initializes @ref _nodeInfo
    * @param [in]  nodeId         : ID of the node; initializes @ref _nodeInfo
    * @param [in]  errorCtx       : Error context for reporting constructor errors
    * @retval None
    * @throw None
    */
   Undb10(T          *pX,
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
    * - index 0: _pX (input data matrix) - managed by setAddr()
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
    * - index 0: _pY (output data matrix) - managed by setAddr()
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

   ~Undb10() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
