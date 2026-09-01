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
 * \class Softclip
 * @brief Provides node-level APIs for the AUDIOLIB_softClip kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel clips the input buffer pX and stores the resulting values to the output buffer pY based on a
 * given threshold value and endKnee value.
 * <br>For each sample in the input channel, the corresponding output value is stored in the output.
 *
 * <b>Example: </b>
 *
 * <b>Input</b> (pX): No. of samples = N; No. of channels = M.
 *  | Channel/Sample |  s0   |   s1   | ... |   sN-1  |
 *  |----------------|-------|--------|-----|---------|
 *  |      ch0       | 9.763 | -7.294 | ... |  0.145  |
 *  |      ch1       | -2.472| -9.428 | ... |  -6.222 |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      .         |   .   |   .    | ... |    .    |
 *  |      chM-1     |-5.155 | -0.501 | ... |  -3.231 |
 *
 * <b>threshld</b> (threshold): 0.5 <br>
 * <b>endKnee</b> (endKnee): 1 <br>
 *
 * <b>Output</b> (pY): No. of samples = N; No. of channels = M.
 *  | Channel/Sample |   s0   |   s1   | ... |  sN-1   |
 *  |----------------|--------|--------|-----|---------|
 *  |      ch0       | 0.974  | -0.965 | ... |  0.145  |
 *  |      ch1       | -0.898 | -0.973 | ... |  -0.959 |
 *  |      .         |    .   |    .   | ... |    .    |
 *  |      .         |    .   |    .   | ... |    .    |
 *  |      .         |    .   |    .   | ... |    .    |
 *  |      chM-1     | -0.951 | -0.501 | ... |  -0.922 |
 *
 */

template <typename T> class Softclip final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input matrix. */
   T       *_pY;       /*!< Pointer to output matrix. */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   size_t _width;
   size_t _height;
   size_t _inPitchBytes;
   size_t _outPitchBytes;
   float  _threshold;
   float  _endKnee;

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_softClip kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_softClip node.
    *
    * This constructor initializes a node for the AUDIOLIB_softClip kernel.
    *
    * @param [in]  *pX            : Pointer to the input matrix.
    *                               Initializes @ref _pX.
    * @param [out] *pY            : Pointer to the output matrix.
    *                               Initializes @ref _pY.
    * @param [in]  width          : Width of input matrix.
    * @param [in]  height         : Height of input matrix.
    * @param [in]  inPitchBytes   : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes  : Pitch of output matrix in bytes.
    * @param [in]  threshold      : The amplitude (linear scale) at which soft clipping begin.
    * @param [in]  endKnee        : The amplitude where the soft clipping curve fully transitions to its maximum limit.
    *                               Between threshold and endKnee, the signal is smoothly compressed
    * @param [in]  errorCtx       : Error context for reporting constructor errors
    *
    */
   Softclip(T         *pX,
            T         *pY,
            size_t     width,
            size_t     height,
            size_t     inPitchBytes,
            size_t     outPitchBytes,
            float      threshold,
            float      endKnee,
            ErrorCtxt &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_softClip node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Softclip(T          *pX,
            T          *pY,
            size_t      width,
            size_t      height,
            size_t      inPitchBytes,
            size_t      outPitchBytes,
            float       threshold,
            float       endKnee,
            const char *nodeName,
            uint32_t    nodeId,
            ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pX (input data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer
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

   ~Softclip() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
