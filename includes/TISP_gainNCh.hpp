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
 * \class GainNCh
 * @brief Provides node-level APIs for the AUDIOLIB_gainNCh kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel applies a specified gain factor to each channel in the input buffer pX
 * (interleaved or non-interleaved) and writes the resulting values to the output buffer pY based on a
 * given array of gain values pG.
 *
 * <b>Example (Interleaved data): </b>
 *
 * <b>Input</b> (pX): No. of samples = N; No. of channels = M.
 *  | Sample\Channel | Ch0 | Ch1 | ... | ChM-1 |
 *  |--------------- |-----|-----|-----|-------|
 *  |      s0        | 100 | 320 | ... |  430  |
 *  |      s1        | 120 | 220 | ... |  630  |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |     sN-1       | 980 |1050 | ... | 1024  |
 *
 * <b>gain values array</b> (pG): `{1, 3, ..., 2}` <br>
 *
 * <b>Output</b> (pY): No. of samples = N; No. of channels = M.
 *  | Sample\Channel | Ch0 | Ch1 | ... | ChM-1 |
 *  |--------------- |-----|-----|-----|-------|
 *  |      s0        | 100 | 960 | ... |  860  |
 *  |      s1        | 120 | 660 | ... | 1260  |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |     sN-1       | 980 |3150 | ... | 2048  |
 *
 **/

/**
 * <b>Runtime Control Philosophy:</b>
 * The GainNCh node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class GainNChParamId : uint16_t {
   kGainValue = 0, /**< kGainValue: Gain values to set/get for each channel */
   kBypass    = 1  /**< kBypass: uint32_t bypass mode; `0` applies the gain, `1` copies input to output */
};

template <typename T> class GainNCh final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input buffer */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pG; /*!< Pointer to input gain array. */
   T *_pY; /*!< Pointer to output matrix. */

   size_t _width;         /*!< Width of the input matrix. */
   size_t _height;        /*!< Height of the input matrix. */
   size_t _gHeight;       /*!< Number of elements in the gain array. */
   size_t _inPitchBytes;  /*!< Pitch of the input matrix in bytes. */
   size_t _outPitchBytes; /*!< Pitch of the output matrix in bytes. */
   size_t _isInterleave;  /*!< `0`: non-interleaved, `1`: interleaved input data. */

   uint32_t _bypassMode; /*!< Bypass mode: `0` applies the gain, `1` copies input to output. */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_gainNCh kernel. */

   /**
    * @brief Copies @ref _height rows of @ref _width elements from @ref _pX to @ref _pY, honouring
    *        @ref _inPitchBytes and @ref _outPitchBytes.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

 public:
   /**
    * @brief Creates an AUDIOLIB_gainNCh node.
    *
    * This constructor initializes a node for the AUDIOLIB_gainNCh kernel.
    *
    * @param [in]  *pX           : Pointer to the input matrix.
    *                              Initializes @ref _pX.
    * @param [in]  *pG           : Pointer to the array of input gain values.
    *                              Initializes @ref _pG.
    * @param [out] *pY           : Pointer to the output matrix.
    *                              Initializes @ref _pY.
    * @param [in]  width         : Width of input matrix.
    * @param [in]  height        : Height of input matrix.
    * @param [in]  gHeight       : Number of elements in input gain array.
    * @param [in]  inPitchBytes  : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes : Pitch of output matrix in bytes.
    * @param [in]  isInterleave  : Indicates whether the input matrix data is interleaved:
    *                            - `0`: Non-interleaved (Rows represent channels, columns represent samples).
    *                            - `1`: Interleaved (Rows represent samples, columns represent channels).
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   GainNCh(T         *pX,
           T         *pG,
           T         *pY,
           size_t     width,
           size_t     height,
           size_t     gHeight,
           size_t     inPitchBytes,
           size_t     outPitchBytes,
           size_t     isInterleave,
           ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_gainNCh node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param[in]  nodeName : Name of the node; initializes @ref _nodeInfo
    * @param[in]  nodeId   : ID of the node; initializes @ref _nodeInfo
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   GainNCh(T          *pX,
           T          *pG,
           T          *pY,
           size_t      width,
           size_t      height,
           size_t      gHeight,
           size_t      inPitchBytes,
           size_t      outPitchBytes,
           size_t      isInterleave,
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
    * @throw None
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
    * @throw None
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

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

   ~GainNCh() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
