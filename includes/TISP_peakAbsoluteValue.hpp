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
 * \class PeakAbsoluteValue
 * @brief Provides node-level APIs for the DSPLIB_peakAbsoluteValue kernel from DSPLIB.
 * @tparam T : Supports int8_t, int16_t, int32_t, int64_t, float and double.
 *
 * @details Finds the largest absolute value of every row of the input and writes one value per row to the output
 *          (the kernel computes `out[r] = max over c of |in[r][c]|`). For a non-interleaved input a row is a
 *          channel, so the output holds one peak per channel; for an interleaved input a row is one sample across
 *          all channels. The output therefore has `width = input height` and `height = 1`. The input row pitch
 *          may be larger than a row. An integer input equal to the most negative value of the type counts as the most
 *          positive value of the type (saturating absolute value), for example the absolute value of -128 is 127 for
 *          int8_t.
 *
 * <b>Example: </b>
 *
 * <b>Input</b>: width = 3; height = 3; T = int16_t; non-interleaved.
 *  | Row/Column | c0  | c1   | c2  |
 *  |------------|-----|------|-----|
 *  |     r0     | 23  | -146 | 79  |
 *  |     r1     | -57 | 131  | 29  |
 *  |     r2     | 38  | -43  | 88  |
 *
 * <b>Output</b>: width = 3 (the input height); height = 1.
 *  | Row/Column | c0  | c1  | c2 |
 *  |------------|-----|-----|----|
 *  |     r0     | 146 | 131 | 88 |
 *
 **/

template <typename T> class PeakAbsoluteValue final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level DSPLIB_peakAbsoluteValue kernel */

 public:
   /**
    * @brief Creates a DSPLIB_peakAbsoluteValue node.
    *
    * @details Validates the buffer descriptors and initializes the DSPLIB_peakAbsoluteValue kernel.
    *
    * @param [in] pInputs  : Input buffer descriptors; exactly one buffer. Its @c pBuf is the initial input
    *                        pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs : Output buffer descriptors; exactly one buffer with width equal to the input
    *                        height and height 1: the maximum absolute value of each input row.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx : Error context for reporting constructor errors
    *
    * @throw None
    */
   PeakAbsoluteValue(const TISP_BufDescList *pInputs,
                     const TISP_BufDescList *pOutputs,
                     const char             *nodeName,
                     uint32_t                nodeId,
                     ErrorCtxt              &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: input buffer (@ref _in) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: maximum absolute value of each input row (@ref _out) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: input buffer)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: maximum absolute value of each input row)
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param [in] addrs : Structure containing source and destination addresses
    * @param [in] mode  : Mode of address setting (input, output, or both)
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
   ~PeakAbsoluteValue() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
