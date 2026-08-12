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
 * \class Delay
 * @brief Provides node-level APIs for the AUDIOLIB_delay kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel copies each channel from the input buffer pX
 * (interleaved or non-interleaved) to the output buffer pY based on a
 * given delay size. <br>For each channel, delay size elements of zeros are stored in the output and then
 * the corresponding input channel is stored in the output.
 *
 * <b>Example (Non-interleaved data): </b>
 *
 * <b>Input</b> (pX): No. of samples = N; No. of channels = M.
 *  | Channel/Sample | s0  | s1  | ... | sN-2  | sN-1  | sN  |
 *  |--------------- |-----|-----|-----|-------|-------|-----|
 *  |      ch0       | 100 | 320 | ... |  430  |  520  | 701 |
 *  |      ch1       | 120 | 220 | ... |  630  |  840  | 620 |
 *  |      .         |  .  |  .  | ... |   .   |   .   |  .  |
 *  |      .         |  .  |  .  | ... |   .   |   .   |  .  |
 *  |      .         |  .  |  .  | ... |   .   |   .   |  .  |
 *  |      chM       | 980 |1050 | ... | 1024  |  240  | 156 |
 *
 * <b>mode</b> (mode): 0 <br>
 * <b>delaySize</b> (delaySize): 2 <br>
 *
 * <b>Output</b> (pY): No. of samples = N; No. of channels = M.
 *  | Channel/Sample | s0  | s1  |  s2 |  s3 | ... | sN  |
 *  |--------------- |-----|-----|-----|-----|-----|-----|
 *  |      ch0       |  0  |  0  | 100 | 320 | ... | 430 |
 *  |      ch1       |  0  |  0  | 120 | 220 | ... | 630 |
 *  |      .         |  .  |  .  | ... |  .  | ... |  .  |
 *  |      .         |  .  |  .  | ... |  .  | ... |  .  |
 *  |      .         |  .  |  .  | ... |  .  | ... |  .  |
 *  |      chM       |  0  |  0  | 980 |1050 | ... |1024 |
 *
 */

template <typename T> class Delay final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input buffer */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pD; /*!< Pointer to input delay matrix. */
   T *_pS; /*!< Pointer to scratch matrix. */
   T *_pY; /*!< Pointer to result matrix. */

   size_t _width;           /*!< Width (samples) of the input matrix. */
   size_t _height;          /*!< Height (channels) of the input matrix. */
   size_t _widthD;          /*!< Width of the delay matrix. */
   size_t _heightD;         /*!< Height (channels) of the delay matrix. */
   size_t _inPitchBytes;    /*!< Pitch in bytes of the input matrix. */
   size_t _outPitchBytes;   /*!< Pitch in bytes of the output matrix. */
   size_t _delayPitchBytes; /*!< Pitch in bytes of the delay matrix. */
   size_t _interleave;      /*!< 0 = non-interleaved, 1 = interleaved. */
   size_t _mode;            /*!< 0 = linear delay, 1 = circular delay. */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_delay kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_delay node.
    *
    * This constructor initializes a node for the AUDIOLIB_delay kernel.
    *
    * @param [in]  *pX           : Pointer to the input matrix.
    *                              Initializes @ref _pX.
    * @param [in]  *pD           : Pointer to the input delay matrix with shape as follows.
    *                            - If mode = '0':
    *                              - Width = delaySize; Height = height.
    *                            - If mode = '1':
    *                              - Width = (delaySize + width); Height = height.
    *                              Initializes @ref _pD.
    * @param [in]  *pS           : Pointer to the intermediate scratch memory with the same shape as the input matrix.
    *                              initializes @ref _pS.
    * @param [out] *pY           : Pointer to the output matrix.
    *                              Initializes @ref _pY.
    * @param [in]  width         : Width of input matrix.
    * @param [in]  height        : Height of input matrix.
    * @param [in]  widthD        : Width of delay matrix.
    * @param [in]  heightD       : Height of delay matrix.
    * @param [in]  inPitchBytes  : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes : Pitch of output matrix in bytes.
    * @param [in]  delayPitchBytes    : Pitch of delay matrix in bytes.
    * @param [in]  interleave    : Indicates whether the input matrix data is interleaved:
    *                            - `0`: Non-interleaved (Rows represent channels, columns represent samples).
    *                            - `1`: Interleaved (Rows represent samples, columns represent channels).
    * @param [in]  mode          : Indicates the type of delay.
    *                            - `0': Linear delay.
    *                            - `1': Circular delay.
    * @param [in]  delaySize     : Number of elements in delay matrix.
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   Delay(T         *pX,
         T         *pD,
         T         *pS,
         T         *pY,
         size_t     width,
         size_t     height,
         size_t     widthD,
         size_t     heightD,
         size_t     inPitchBytes,
         size_t     delayPitchBytes,
         size_t     outPitchBytes,
         size_t     interleave,
         size_t     mode,
         size_t     delaySize,
         ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_delay node with a name and ID
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
   Delay(T          *pX,
         T          *pD,
         T          *pS,
         T          *pY,
         size_t      width,
         size_t      height,
         size_t      widthD,
         size_t      heightD,
         size_t      inPitchBytes,
         size_t      delayPitchBytes,
         size_t      outPitchBytes,
         size_t      interleave,
         size_t      mode,
         size_t      delaySize,
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
    * @brief Clean and reset all internal state and history buffers to zero
    *
    * @retval None
    * @throw None
    */
   void cleanInternalBuffers() noexcept override;

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    * @retval None
    * @throw None
    */

   ~Delay() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
