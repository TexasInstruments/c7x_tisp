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
 * \class Tablelookup
 * @brief Provides node-level APIs for the AUDIOLIB_tableLookup kernel from AUDIOLIB.
 * @tparam TIn  : Source sample type, float or int32_t.
 * @tparam TOut : Table and output type; float or int32_t for an int32_t source, float for a float source.
 *                Defaults to @c TIn.
 *
 * @details Replaces every source sample by an entry of a common lookup table of
 *          @ref TablelookupConfig::tableSamples entries:
 *          - float source: the sample is mapped linearly from [@c minVal, @c maxVal] onto the table index range,
 *            rounded to the nearest index and clamped to the table.
 *          - int32_t source: the sample is the table index itself, clamped to [0, tableSamples - 1];
 *            @c minVal and @c maxVal are not used.
 *
 *          The kernel works on one contiguous run of samples, so the node takes every element of the input
 *          buffer in memory order: rows must be packed (pitch equal to one row) unless the buffer has one row.
 *          Input and output have the same dimensions; the layout does not change the result.
 *
 *          Supported combinations (source, table/output): (float, float), (int32_t, float), (int32_t, int32_t).
 **/

/**
 * @brief Construction-time (non-tunable) parameters of the @ref Tablelookup node.
 */
struct TablelookupConfig {
   size_t   tableSamples    = 0U;   /*!< Number of entries in the table; required, at least 1. */
   float    minVal          = 0.0F; /*!< Float source value mapped to the first table entry; unused for int32_t. */
   float    maxVal          = 1.0F; /*!< Float source value mapped to the last table entry; unused for int32_t. */
   uint32_t tableLookupSize = 256U; /*!< Entries of the device lookup table. On C7524 a table of at most this many
                                         entries is loaded into the hardware lookup table; larger tables are read
                                         from memory. At least 1. */
};

/**
 * @brief Tunable parameters of the @ref Tablelookup node.
 *
 * @details Tablelookup has no runtime parameters (no setParams / getParams); this struct carries the table the
 *          node is constructed with.
 *
 * @tparam TOut : Table and output type.
 */
template <typename TOut> struct TablelookupTunableParams {
   TOut *pTable = nullptr; /*!< Caller-owned table of tableSamples entries; required, must outlive the node. */
};

template <typename TIn, typename TOut = TIn> class Tablelookup final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   TOut          *_table;    /*!< Caller-owned table (tableSamples entries), read by exec */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_tableLookup kernel */

 public:
   /**
    * @brief Creates an AUDIOLIB_tableLookup node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_tableLookup kernel for
    *          width * height samples.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one buffer of @c TIn with packed rows.
    *                                    Its @c pBuf is the initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one buffer of @c TOut with packed rows
    *                                    and the dimensions of the input.
    * @param [in] pConfig          : Construction-time parameters; required.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with a non-null @c pTable.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   Tablelookup(const TISP_BufDescList               *pInputs,
               const TISP_BufDescList               *pOutputs,
               const TablelookupConfig              *pConfig,
               const TablelookupTunableParams<TOut> *pDefaultTunableParams,
               const char                           *nodeName,
               uint32_t                              nodeId,
               ErrorCtxt                            &errorCtx);

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
    * - index 0: output buffer (@ref _out) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1)
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
   ~Tablelookup() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
