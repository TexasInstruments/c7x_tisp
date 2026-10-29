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
 * \class SubBlockStatistics
 * @brief Provides node-level APIs for the AUDIOLIB_subBlockStatistics kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel performs a specified statistical operation on the sub blocks of the input buffer pX across each
 * channel and writes the resulting values to the output buffer pY based on a given statisticsType parameter and
 * flagPerChanStatus. <br><br>If flagPerChanStatus = 0, then output size = number of sub blocks per channel. <br>If
 * flagPerChanStatus = 1, then output size = heightIn * number of sub blocks per channel. <br>Number of sub blocks per
 * channel is calculated by dividing the widthIn by subBlockSize.
 *
 * <b>StatisticsType: </b>
 *
 *  | StatisticsType |         Operation        |
 *  |----------------|--------------------------|
 *  |       0        |          Maximum         |
 *  |       1        |          Minimum         |
 *  |       2        |  Maximum Absolute Value  |
 *  |       3        |           Mean           |
 *  |       4        |           RMS            |
 *  |       5        |    Standard Deviation    |
 *  |       6        |         Variance         |
 *  |       7        |      Average Energy      |
 *  |       8        |           Sum            |
 *  |       9        |      Sum of squares      |
 *
 * <b>Example: </b>
 *
 * <b>Input</b> (pX): No. of samples = 3; No. of channels = M; subBlockSize = 1; flagPerChanStatus = 0.
 *  | Channel/Sample | s0  | s1  | s2  |
 *  |--------------- |-----|-----|-----|
 *  |      ch0       | 100 | 320 | 430 |
 *  |      ch1       | 630 | 220 | 120 |
 *  |      .         |  .  |  .  |  .  |
 *  |      .         |  .  |  .  |  .  |
 *  |      .         |  .  |  .  |  .  |
 *  |      chM-1     | 980 |1050 |1024 |
 *
 * <b>statisticsType</b> (statisticsTye): 0 <br>
 * Number of sub blocks per channel = widthIn / subBlockSize = 3 / 1 = 3.<br>
 *
 * <b>Output</b> (pY): No. of samples = 3; No. of channels = 1.
 *  | Channel/Sample | s0  |  s1  |  s2  |
 *  |--------------- |-----|------|------|
 *  |      ch0       | 980 | 1050 | 1024 |
 *
 */

template <typename T> class SubBlockStatistics final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input buffer */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pY; /*!< Pointer to output matrix. */

   size_t _widthIn;
   size_t _heightIn;
   size_t _inPitchBytes;
   size_t _outPitchBytes;
   size_t _subBlockSize;
   size_t _flagPerChanStats;
   size_t _statisticsType;

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_subBlockStatistics kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_subBlockStatistics node.
    *
    * This constructor initializes a node for the AUDIOLIB_subBlockStatistics kernel.
    *
    * @param [in]  pX               : Pointer to the input matrix.
    *                                 Initializes @ref _pX.
    * @param [out] pY               : Pointer to the output matrix.
    *                                 Initializes @ref _pY.
    * @param [in]  widthIn          : Width of input matrix.
    * @param [in]  heightIn         : Height of input matrix.
    * @param [in]  inPitchBytes     : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes    : Pitch of output matrix in bytes.
    * @param [in]  subBlockSize     : Number of samples in a sub-block per channel.
    * @param [in]  flagPerChanStats : Selects channel processing mode.
    *                               - '0': for all-channel statistics (single-channel output).
    *                               - '1': for per-channel statistics (multi-channel output).
    * @param [in]  statisticsType   : Selects the statistic to compute for each channel.
    *                               - `0`: Maximum,
    *                               - `1`: Minimum,
    *                               - `2`: Maximum Absolute Value,
    *                               - `3`: Mean,
    *                               - `4`: RMS,
    *                               - `5`: Standard Deviation,
    *                               - `6`: Variance,
    *                               - `7`: Average Energy,
    *                               - `8`: Sum,
    *                               - `9`: Sum of squares.
    * @param [in]  errorCtx         : Error context for reporting constructor errors
    *
    */
   SubBlockStatistics(T         *pX,
                      T         *pY,
                      size_t     widthIn,
                      size_t     heightIn,
                      size_t     inPitchBytes,
                      size_t     outPitchBytes,
                      size_t     subBlockSize,
                      size_t     flagPerChanStats,
                      size_t     statisticsType,
                      ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_subBlockStatistics node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   SubBlockStatistics(T          *pX,
                      T          *pY,
                      size_t      widthIn,
                      size_t      heightIn,
                      size_t      inPitchBytes,
                      size_t      outPitchBytes,
                      size_t      subBlockSize,
                      size_t      flagPerChanStats,
                      size_t      statisticsType,
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

   ~SubBlockStatistics() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
