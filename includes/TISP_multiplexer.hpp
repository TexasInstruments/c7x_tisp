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
 * \class Multiplexer
 * @brief Provides node-level APIs for the AUDIOLIB_multiplexer kernel from AUDIOLIB.
 * @tparam T : Supports float, double.
 *
 * @details This kernel routes selected channels from the input buffer pX
 * (interleaved or non-interleaved) to the output buffer pY based on a
 * given array of channel indices pC. <br>For each channel index, the corresponding input channel is stored in the
 * output. If an index exceeds the available input channels, the output channel is filled with zeros.
 *
 * It supports both interleaved and non-interleaved formats of data.
 *
 * <b>Example (Non-Interleaved data): </b>
 *
 * <b>Input</b> (pX): No. of samples = N; No. of channels = M, No. of inputs = 3.
 *  Input 0:
 *  | Channel/Sample |  s0   |   s1   | ... |   sN-1  |
 *  |----------------|-------|--------|-----|---------|
 *  |      ch0       |  120  |   100  | ... |   145   |
 *  |      ch1       |  340  |   723  | ... |   368   |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      chM-1     |  210  |   625  | ... |   271   |
 *
 *  Input 1:
 *  | Channel/Sample |  s0   |   s1   | ... |   sN-1  |
 *  |----------------|-------|--------|-----|---------|
 *  |      ch0       |  720  |   190  | ... |   222   |
 *  |      ch1       |  436  |   536  | ... |   890   |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      chM-1     |  839  |   702  | ... |   345   |
 *
 *  Input 2:
 *  | Channel/Sample |  s0   |   s1   | ... |   sN-1  |
 *  |----------------|-------|--------|-----|---------|
 *  |      ch0       |  246  |   172  | ... |   195   |
 *  |      ch1       |  193  |   840  | ... |   208   |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      chM-1     |  289  |   107  | ... |   156   |
 *
 * <b>Input index value</b> (inputIdx): 2 <br>
 *
 * <b>Output</b> (pY): No. of samples = N; No. of channels = M.
 *  | Channel/Sample |  s0   |   s1   | ... |   sN-1  |
 *  |----------------|-------|--------|-----|---------|
 *  |      ch0       |  720  |   190  | ... |   222   |
 *  |      ch1       |  436  |   536  | ... |   890   |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      chM-1     |  839  |   702  | ... |   345   |
 *
 *
 * <b>Input index value</b> (inputIdx): 5 <br>
 *
 * <b>Output</b> (pY): No. of samples = N; No. of channels = M.
 *  | Channel/Sample |  s0   |   s1   | ... |   sN-1  |
 *  |----------------|-------|--------|-----|---------|
 *  |      ch0       |  120  |   100  | ... |   145   |
 *  |      ch1       |  340  |   723  | ... |   368   |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      chM-1     |  210  |   625  | ... |   271   |
 *
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The Multiplexer node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class MultiplexerParamId : uint16_t {
   kSrcIndex = 0 /**< uint32_t: Index to the required source */
};

template <typename T> class Multiplexer final : public VirtualOp {

   T *_pX; /*!< Pointer to input matrix. */
   T *_pY; /*!< Pointer to output matrix. */

   size_t _height;
   size_t _inputs;
   size_t _inPitchBytesY;
   size_t _inputIdx;

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_multiplexer kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_multiplexer node.
    *
    * This constructor initializes a node for the AUDIOLIB_multiplexer kernel.
    *
    * @param [in]  pX             : Pointer to the input matrix. Interpretation of rows and columns depends on
    * `isInterleave`. Initializes @ref _pX.
    * @param [out] pY             : Pointer to the output matrix. Interpretation of rows and columns depends on
    * `isInterleave`. Initializes @ref _pY.
    * @param [in]  width          : Width of input matrix.
    * @param [in]  height         : Height of input matrix.
    * @param [in]  inputs         : Length of input matrix in Z dimension.
    * @param [in]  inPitchBytesY  : Y dimension Pitch of input matrix in bytes.
    * @param [in]  inPitchBytesZ  : Z dimension Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes  : Pitch of output matrix in bytes.
    * @param [in]  currentIdx     : Index of input matrix.
    * @param [in]  errorCtx       : Error context for reporting constructor errors
    *
    */
   Multiplexer(T         *pX,
               T         *pY,
               size_t     width,
               size_t     height,
               size_t     inputs,
               size_t     inPitchBytesY,
               size_t     inPitchBytesZ,
               size_t     outPitchBytes,
               size_t     currentIdx,
               ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_multiplexer node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */

   Multiplexer(T          *pX,
               T          *pY,
               size_t      width,
               size_t      height,
               size_t      inputs,
               size_t      inPitchBytesY,
               size_t      inPitchBytesZ,
               size_t      outPitchBytes,
               size_t      currentIdx,
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
    * @param [in] addrs  : Structure containing source and destination addresses
    * @param [in] mode   : Mode of address setting (input, output, or both)
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Update runtime parameters with detailed error reporting
    *
    * Updates the specified runtime parameter by ID with the data provided in the value buffer.
    * The node strictly validates that the provided `size` argument matches or exceeds the
    * expected byte size for the given parameter before mapping the state into the active node memory.
    *
    * @param [in] paramId      : Parameter ID to update
    * @param [in] value        : Pointer to a buffer containing the new parameter value
    * @param [in] size         : The size in bytes of the payload
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true         : Parameters updated successfully
    * @retval false        : Failed to update parameters
    */
   bool setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept override;

   /**
    * @brief Retrieve current runtime parameters with detailed error reporting
    *
    * This API fetches the current live state from the node for the specified parameter ID
    * and writes it into the provided buffer.
    *
    * If the user provides a buffer, it populates the requested data. If the buffer is too small
    * (or `value` is `nullptr`), it safely fails (returns `false`) but injects the exact required
    * byte capacity into `*size`, allowing the application to re-allocate precisely.
    *
    * @param [in] paramId      : Parameter ID to query
    * @param [out] value       : Pointer to a buffer where parameters will be stored
    * @param [in,out] size     : Input: Capacity of value buffer in bytes. Output: Actual bytes written or required.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true          : Parameters retrieved successfully
    * @retval false         : Failed to retrieve parameters
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

   /**
    * @brief Execute the operation
    *
    * This function performs the actual operation.
    *
    */

   void exec() noexcept override;

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
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~Multiplexer() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
