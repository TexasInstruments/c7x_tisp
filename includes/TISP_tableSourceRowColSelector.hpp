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
 * \class TableSourceRowColSelector
 * @brief Provides node-level APIs for the DSPLIB_tableSourceRowColSelector kernel from DSPLIB.
 * @tparam T : Table and output type; supports int32_t, int64_t, uint32_t, uint64_t, float and double. The index
 *             input is always int32_t.
 *
 * @details Copies, for every index of the input, the table row or column selected by that index to the output. The
 *          table is a packed row-major matrix of @ref TableSourceRowColSelectorConfig::numRows rows and
 *          @ref TableSourceRowColSelectorConfig::numCols columns: element (row, col) is at
 *          `pTable[row * numCols + col]`.
 *          - Row select (@ref TableSourceRowColSelectorConfig::isColSelect = 0): an index selects a table row, clamped
 *            to [0, numRows - 1]; the selected vector has selLen = numCols elements.
 *          - Column select (isColSelect = 1): an index selects a table column, clamped to [0, numCols - 1]; the
 *            selected vector has selLen = numRows elements.
 *
 *          The output layout follows the output descriptor's @c isInterleave flag:
 *          - Non-interleaved (0): one output row per index; width = selLen, height = number of indices.
 *          - Interleaved (1): one output column per index; width = number of indices, height = selLen. The kernel
 *            writes the selected vectors to a scratch buffer and transposes it into the output, so this layout needs
 *            a scratch buffer of at least @ref calcScratchSize bytes.
 *
 *          The kernel reads the indices as one contiguous run, so the node takes every element of the input buffer
 *          in memory order: rows must be packed (pitch equal to one row) unless the buffer has one row. The output
 *          row pitch may be larger than a row.
 *
 * @image html tableSourceRowColSelector_node_kernel.svg "TableSourceRowColSelector node block diagram" width=800px
 *
 * <b>Example: </b>
 *
 * <b>Table</b>: numRows = 3; numCols = 2; T = float.
 *  | Row/Column | c0  | c1  |
 *  |------------|-----|-----|
 *  |     r0     | 0.1 | 0.2 |
 *  |     r1     | 1.1 | 1.2 |
 *  |     r2     | 2.1 | 2.2 |
 *
 * <b>Input</b>: width = 4; height = 1; int32_t indices.
 *  | Row/Column | c0 | c1 | c2 | c3 |
 *  |------------|----|----|----|----|
 *  |     r0     | 2  | 0  | 7  | -1 |
 *
 * <b>Output, row select, non-interleaved</b>: width = 2; height = 4 (one row per index; 7 is clamped to 2 and -1
 * to 0).
 *  | Row/Column | c0  | c1  |
 *  |------------|-----|-----|
 *  |     r0     | 2.1 | 2.2 |
 *  |     r1     | 0.1 | 0.2 |
 *  |     r2     | 2.1 | 2.2 |
 *  |     r3     | 0.1 | 0.2 |
 *
 * <b>Output, column select, interleaved</b>: width = 4; height = 3 (one column per index; 2 and 7 are clamped to 1
 * and -1 to 0).
 *  | Row/Column | c0  | c1  | c2  | c3  |
 *  |------------|-----|-----|-----|-----|
 *  |     r0     | 0.2 | 0.1 | 0.2 | 0.1 |
 *  |     r1     | 1.2 | 1.1 | 1.2 | 1.1 |
 *  |     r2     | 2.2 | 2.1 | 2.2 | 2.1 |
 **/

/**
 * @brief Construction-time (non-tunable) parameters of the @ref TableSourceRowColSelector node.
 */
struct TableSourceRowColSelectorConfig {
   size_t  numRows     = 0U; /*!< Number of rows of the table; required, at least 1. */
   size_t  numCols     = 0U; /*!< Number of columns of the table; required, at least 1. */
   uint8_t isColSelect = 0U; /*!< `0`: an index selects a table row, `1`: an index selects a table column. */
};

/**
 * @brief Tunable parameters of the @ref TableSourceRowColSelector node.
 *
 * @details TableSourceRowColSelector has no runtime parameters (no setParams / getParams); this struct carries the
 *          table the node is constructed with.
 *
 * @tparam T : Table and output type.
 */
template <typename T> struct TableSourceRowColSelectorTunableParams {
   T *pTable = nullptr; /*!< Caller-owned packed table of numRows x numCols entries; required, must outlive the
                             node. */
};

template <typename T> class TableSourceRowColSelector final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input (index) buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   T             *_table;    /*!< Caller-owned table (numRows x numCols entries), read by exec */
   void          *_scratch;  /*!< Caller-owned scratch buffer, used only by the interleaved output */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level DSPLIB_tableSourceRowColSelector kernel */

 public:
   /**
    * @brief Creates a DSPLIB_tableSourceRowColSelector node.
    *
    * @details Validates the buffer descriptors and initializes the DSPLIB_tableSourceRowColSelector kernel for
    *          width * height indices.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one buffer of int32_t indices with packed
    *                                    rows. Its @c pBuf is the initial input pointer, rebound later by
    *                                    @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one buffer of @c T. Non-interleaved: width
    *                                    selLen, height number of indices. Interleaved: width number of indices,
    *                                    height selLen.
    * @param [in] pScratch              : Scratch buffer used by the kernel during exec; required for the interleaved
    *                                    output, may be nullptr for the non-interleaved output.
    * @param [in] scratchBytes          : Size of @c pScratch in bytes; for the interleaved output at least
    *                                    @ref calcScratchSize (number of indices, selLen).
    * @param [in] pConfig               : Construction-time parameters; required.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with a non-null @c pTable.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   TableSourceRowColSelector(const TISP_BufDescList                          *pInputs,
                             const TISP_BufDescList                          *pOutputs,
                             void                                            *pScratch,
                             size_t                                           scratchBytes,
                             const TableSourceRowColSelectorConfig           *pConfig,
                             const TableSourceRowColSelectorTunableParams<T> *pDefaultTunableParams,
                             const char                                      *nodeName,
                             uint32_t                                         nodeId,
                             ErrorCtxt                                       &errorCtx);

   /**
    * @brief Size of the scratch buffer the interleaved output needs.
    *
    * @details The kernel writes one scratch row per index and reads the scratch transposed, one vector of indices at
    *          a time, so both dimensions are rounded up to a whole vector (@ref TISP_SE_SA_ALIGN_BYTES / sizeof(T)
    *          elements, the widest C7x vector): roundup(numIndices, lanes) x roundup(selLen, lanes) elements.
    *
    * @param [in] numIndices : Number of indices (input width x height)
    * @param [in] selLen     : Length of a selected vector: numCols for row select, numRows for column select
    * @retval     size_t     : Scratch size in bytes
    */
   static size_t calcScratchSize(size_t numIndices, size_t selLen) noexcept;

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: indices (@ref _in) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: selected table rows or columns (@ref _out) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: indices)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: selected table rows or columns)
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
   ~TableSourceRowColSelector() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
