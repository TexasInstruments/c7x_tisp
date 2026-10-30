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
 * @brief Construction-time (non-tunable) parameters of the @ref Recip node.
 *
 * @details Fixed for the node's lifetime; required.
 */
struct RecipConfig {
   uint32_t storeStyle =
       0U; /*!< Output store style: LINEAR for float and double; HSTACK (exp0..expN-1, frac0..fracN-1) or INTERLEAVE
              (exp0, frac0, exp1, frac1, ...) for int16 (0 = LINEAR, 1 = HSTACK, 2 = INTERLEAVE) */
};

/**
 * \class Recip
 * @brief Provides node-level APIs for the DSPLIB_recip kernel from DSPLIB.
 * @tparam T : Supports int16_t, float and double.
 *
 * @details Computes the reciprocal of every element of the two-dimensional input. The input and output rows have
 *          independent pitches and the width and height are those of the buffer descriptors (for an interleaved
 *          input the width is the channel count). Input and output layouts must match.
 *
 *  <b>float and double: </b> `out = 1 / in`. The output has the same width and height as the input and
 *  @ref RecipConfig::storeStyle must be 0 (LINEAR).
 *
 *  <b>int16_t (Q15): </b> every input element gives two values, the exponent and the fraction of its reciprocal, so
 *  each output row holds twice the input width and the output height equals the input height.
 *  @ref RecipConfig::storeStyle selects where the two values of an element are stored and must be 1 or 2:
 *
 *  | storeStyle | Name       | Output row of an input row with N elements |
 *  |------------|------------|--------------------------------------------|
 *  |     1      | HSTACK     | exp0 .. expN-1, frac0 .. fracN-1           |
 *  |     2      | INTERLEAVE | exp0, frac0, exp1, frac1, ..               |
 *
 * <b>Example (int16_t, HSTACK): </b>
 *
 * <b>Input</b>: width = 3; height = 2.
 *  | Row/Column | c0    | c1     | c2    |
 *  |------------|-------|--------|-------|
 *  |     r0     | 16384 | 8192   | -4096 |
 *  |     r1     | 1000  | -32768 | 3     |
 *
 * <b>Output</b>: width = 6; height = 2.
 *  | Row/Column | c0 | c1 | c2 | c3    | c4     | c5     |
 *  |------------|----|----|----|-------|--------|--------|
 *  |     r0     | 1  | 2  | 3  | 32767 | 32767  | -32767 |
 *  |     r1     | 6  | 0  | 14 | 16777 | -32767 | 21845  |
 *
 * (The output values are those of the Python reference of the DSPLIB_recip test data generator.)
 **/

template <typename T> class Recip final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */

   TISP::HandlePtr<char> _handle; /*!< Handle to the low level kernel called in exec() */

 public:
   /**
    * @brief Creates a Recip node.
    *
    * @details Validates the buffer descriptors and parameters, then initializes the node.
    *
    * @param [in] pInputs  : Input buffer descriptors; exactly 1 buffer: [0] interleaved or planar.
    * @param [in] pOutputs : Output buffer descriptors; exactly 1 buffer: [0] same layout as the input; width and
    *                        height as described in the class details.
    * @param [in] pConfig  : Construction-time parameters; required.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx : Error context for reporting constructor errors
    *
    * @throw None
    */
   Recip(const TISP_BufDescList *pInputs,
         const TISP_BufDescList *pOutputs,
         const RecipConfig      *pConfig,
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
   ~Recip() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
