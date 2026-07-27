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
 * \class GainNChTrim
 * @brief Provides node-level APIs for the AUDIOLIB_gainNChTrim kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel applies a specified gain factor to each channel in the input buffer pX
 * (interleaved or non-interleaved) and multiplies the obtained values with master gain value pM
 * and writes the resulting values to the output buffer pY based on a given array of gain values pG.
 *
 * <b>Example (Interleaved data): </b>
 *
 * <b>Input</b> (pX): No. of samples = N; No. of channels = M.
 *  | SampleChannel | ch0 | ch1 | ... | chM-1 |
 *  |--------------- |-----|-----|-----|-------|
 *  |      s0        | 100 | 320 | ... |  430  |
 *  |      s1        | 120 | 220 | ... |  630  |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |     sN-1       | 980 |1050 | ... | 1024  |
 *
 * <b>gain values array</b> (pG): `{1, 3, ..., 2}` <br>
 * <b>master gain value</b> (pM): 4 <br>
 *
 * <b>Operation:</b>
 * output[s0][ch0] = (input[s0][ch0]*gain[ch0])*masterGain
 *
 * <b>Output</b> (pY): No. of samples = N; No. of channels = M.
 *  | SampleChannel |  Ch0 |  Ch1 | ... | ChM-1 |
 *  |--------------- |------|------|-----|-------|
 *  |      s0        |  400 | 3840 | ... | 3440  |
 *  |      s1        |  480 | 2640 | ... | 5040  |
 *  |      .         |   .  |   .  | ... |   .   |
 *  |      .         |   .  |   .  | ... |   .   |
 *  |      .         |   .  |   .  | ... |   .   |
 *  |     sN-1       | 3920 |12600 | ... | 8192  |
 *
 * <b>Configuration & Parameters:</b>
 *
 * | Parameter    | Configuration Time | Description                                                         |
 * |--------------|--------------------|---------------------------------------------------------------------|
 * | gHeight      | Instance Creation  | Number of channels (gain array size). <b>FIXED</b> at creation time.|
 * | Gain values  | Runtime            | Gain factor for each channel.                                       |
 * | Master gain  | Runtime            | Master gain factor.                                                 |
 * | Bypass       | Runtime            | Copies input to output, leaving the gain unapplied.                 |
 *
 **/

/**
 * <b>Runtime Control Philosophy:</b>
 * The GainNChTrim node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class GainNChTrimParamId : uint16_t {
   kGainValues = 0, /**< kGainValues: Gain values to set/get for each channel */
   kMasterGain = 1, /**< kMasterGain: Master gain scalar (float) to set/get */
   kBypass     = 2  /**< kBypass: uint32_t bypass mode; `0` applies the gain, `1` copies input to output */
};

template <typename T> class GainNChTrim final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input buffer */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pG; /*!< Pointer to input gain array. */
   T *_pM; /*!< Pointer to input master gain value. */
   T *_pY; /*!< Pointer to output matrix. */

   size_t _width;         /*!< Width of the input matrix. */
   size_t _height;        /*!< Height of the input matrix. */
   size_t _gHeight;       /*!< Number of elements in the gain array. */
   size_t _inPitchBytes;  /*!< Pitch of the input matrix in bytes. */
   size_t _outPitchBytes; /*!< Pitch of the output matrix in bytes. */
   size_t _isInterleave;  /*!< `0`: non-interleaved, `1`: interleaved input data. */

   uint32_t _bypassMode; /*!< Bypass mode: `0` applies the gain, `1` copies input to output. */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_gainNChTrim kernel. */

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
    * @brief Creates an AUDIOLIB_gainNChTrim node.
    *
    * This constructor initializes a node for the AUDIOLIB_gainNChTrim kernel.
    *
    * @param [in]  *pX           : Pointer to the input matrix.
    *                              Initializes @ref _pX.
    * @param [in]  *pG           : Pointer to the array of input gain values.
    *                              Initializes @ref _pG.
    * @param [in]  *pM           : Pointer to the input master gain value.
    *                              Initializes @ref _pM.
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
    *
    */
   GainNChTrim(T         *pX,
               T         *pG,
               T         *pM,
               T         *pY,
               size_t     width,
               size_t     height,
               size_t     gHeight,
               size_t     inPitchBytes,
               size_t     outPitchBytes,
               size_t     isInterleave,
               ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_gainNChTrim node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   GainNChTrim(T          *pX,
               T          *pG,
               T          *pM,
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
    * @brief Execute the operation
    *
    * This function performs the actual operation.
    *
    */
   void exec() noexcept override;

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

   ~GainNChTrim() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
