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
 * \class LogicalNOT
 * @brief Provides node-level APIs for the DSPLIB_logicalNOT kernel from DSPLIB.
 * @tparam T : Supports int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float and double.
 *
 * @details Computes the element-wise logical NOT (Boolean invert) of the input: an output element is `1` where the
 *          corresponding input element is zero and `0` where it is non-zero. The output has the type of the input.
 *          Input and output share one layout (interleaved or non-interleaved), width and height; their row
 *          pitches may differ.
 *
 * <b>Example: </b>
 *
 * <b>Input</b>: width = 3; height = 3; T = int16_t.
 *  | Row/Column | c0  | c1   | c2  |
 *  |------------|-----|------|-----|
 *  |     r0     | 0   | -146 | 79  |
 *  |     r1     | -57 | 0    | 29  |
 *  |     r2     | 38  | -43  | 0   |
 *
 * <b>Output</b>: width = 3; height = 3.
 *  | Row/Column | c0  | c1   | c2  |
 *  |------------|-----|------|-----|
 *  |     r0     | 1   | 0    | 0   |
 *  |     r1     | 0   | 1    | 0   |
 *  |     r2     | 0   | 0    | 1   |
 **/

template <typename T> class LogicalNOT final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */

   TISP::HandlePtr<char> _handle; /*!< Handle to the low level kernel called in exec() */

 public:
   /**
    * @brief Creates a DSPLIB_logicalNOT node.
    *
    * @details Validates the buffer descriptors and initializes the DSPLIB_logicalNOT kernel. The kernel checks that
    *          the output width and height match the input.
    *
    * @param [in] pInputs  : Input buffer descriptors; exactly one buffer. Its @c pBuf is the
    *                        initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs : Output buffer descriptors; exactly one buffer with the same layout, width and
    *                        height as the input.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx : Error context for reporting constructor errors
    *
    * @throw None
    */
   LogicalNOT(const TISP_BufDescList *pInputs,
              const TISP_BufDescList *pOutputs,
              const char             *nodeName,
              uint32_t                nodeId,
              ErrorCtxt              &errorCtx);

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
   ~LogicalNOT() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
