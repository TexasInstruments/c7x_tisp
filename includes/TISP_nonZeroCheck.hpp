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
 * @brief Construction-time (non-tunable) parameters of the @ref NonZeroCheck node.
 *
 * @details Fixed for the node's lifetime; required.
 */
struct NonZeroCheckConfig {
   uint32_t opType   = 0U; /*!< Reduction: ALL non-zero or ANY non-zero (0 = ALL, 1 = ANY) */
   uint32_t axisType = 0U; /*!< Axis: ROW reduces down the rows (one result per column position), COLUMN reduces across
                              each row (0 = ROW, 1 = COLUMN) */
};

/**
 * \class NonZeroCheck
 * @brief Provides node-level APIs for the DSPLIB_nonZeroCheck kernel from DSPLIB.
 * @tparam T : Supports int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float and double.
 *
 * @details Reduces the two-dimensional input to one row of results with a logical ALL or ANY: an output element is
 *          `1` if all (ALL) or any (ANY) of the input elements it covers are non-zero, otherwise `0`. The output has
 *          the type of the input, one row (`height = 1`), and the reduction is chosen at construction with
 *          @ref NonZeroCheckConfig::opType and @ref NonZeroCheckConfig::axisType. The width and height are
 *          those of the buffer descriptors (for an interleaved input the width is the channel count). Input and
 *          output layouts are independent.
 *
 *  <b>Reduction (@ref NonZeroCheckConfig::opType): </b>
 *
 *  | opType | Operation | Output element is 1 if              |
 *  |--------|-----------|-------------------------------------|
 *  |   0    | ALL       | all covered input elements non-zero |
 *  |   1    | ANY       | any covered input element non-zero  |
 *
 *  <b>Axis (@ref NonZeroCheckConfig::axisType): </b>
 *
 *  | axisType | Name   | Output width | Output element j covers          |
 *  |----------|--------|--------------|----------------------------------|
 *  |    0     | ROW    | input width  | column j (all rows of the input) |
 *  |    1     | COLUMN | input height | row j (all columns of the input) |
 *
 * <b>Example (ANY, COLUMN): </b>
 *
 * <b>Input</b>: width = 3; height = 3; T = int16_t.
 *  | Row/Column | c0 | c1 | c2 |
 *  |------------|----|----|----|
 *  |     r0     | 0  | 0  | 0  |
 *  |     r1     | 0  | 5  | 0  |
 *  |     r2     | 7  | 8  | 9  |
 *
 * <b>Output</b>: width = 3; height = 1.
 *  | Row/Column | c0 | c1 | c2 |
 *  |------------|----|----|----|
 *  |     r0     | 0  | 1  | 1  |
 **/

template <typename T> class NonZeroCheck final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */

   TISP::HandlePtr<char> _handle; /*!< Handle to the low level kernel called in exec() */

 public:
   /**
    * @brief Creates a NonZeroCheck node.
    *
    * @details Validates the buffer descriptors and parameters, then initializes the node.
    *
    * @param [in] pInputs  : Input buffer descriptors; exactly 1 buffer: [0] interleaved or planar.
    * @param [in] pOutputs : Output buffer descriptors; exactly 1 buffer: [0] interleaved or planar.
    * @param [in] pConfig  : Construction-time parameters; required.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx : Error context for reporting constructor errors
    *
    * @throw None
    */
   NonZeroCheck(const TISP_BufDescList   *pInputs,
                const TISP_BufDescList   *pOutputs,
                const NonZeroCheckConfig *pConfig,
                const char               *nodeName,
                uint32_t                  nodeId,
                ErrorCtxt                &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: input buffer - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: output buffer - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * @param [in] addrs  : Structure containing source and destination addresses
    * @param [in] mode   : Mode of address setting (input, output, or both)
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute the operation
    */
   void exec() noexcept override;

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
    * @retval const NodeBufParams& : The node's i/o buffer properties
    */
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Destructor
    */
   ~NonZeroCheck() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
