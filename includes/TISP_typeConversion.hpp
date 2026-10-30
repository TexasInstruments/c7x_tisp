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
 * @brief Construction-time (non-tunable) parameters of the @ref TypeConversion node.
 *
 * @details Required; selects the fixed-point scaling.
 */
struct TypeConversionConfig {
   uint8_t testQ31 = 0U; /*!< int32_t conversions only: `1` uses a 2^31 (Q31) scale, `0` a 2^23 (Q23) scale. */
};

/**
 * \class TypeConversion
 * @brief Provides node-level APIs for the AUDIOLIB_typeConversion kernel from AUDIOLIB.
 * @tparam Tin  : Supports int16_t, int32_t, float and double.
 * @tparam Tout : Supports int16_t, int32_t, float and double.
 *
 * @details This kernel performs data type conversion on the input buffer across all channels
 * and writes the converted values to the output buffer.
 *
 *  <b>Type Conversions: </b>
 *
 *  |    Input data type   |    Output data type   |
 *  |----------------------|-----------------------|
 *  |       int16_t        |          float        |
 *  |       int16_t        |          double       |
 *  |       int32_t        |          float        |
 *  |       int32_t        |          double       |
 *  |       float          |          int16_t      |
 *  |       float          |          int32_t      |
 *  |       double         |          int16_t      |
 *  |       double         |          int32_t      |
 *
 * <b>Example: </b>
 *
 * <b>Input</b>: No. of samples = N; No. of channels = M; Tin = int32_t, Tout = float.
 *  | Channel/Sample | s0  | s1  | ... | sN-1  |
 *  |----------------|-----|-----|-----|-------|
 *  |      ch0       | 100 | 215 | ... |  675  |
 *  |      ch1       | 30  | 50  | ... |  20   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      chM-1     | 900 |1050 | ... |  105  |
 *
 * <b>Output</b>: No. of samples = N; No. of channels = M.
 *  | Channel/Sample |  s0   |  s1   | ... |  sN-1   |
 *  |----------------|-------|-------|-----|---------|
 *  |      ch0       | 100.0 | 215.0 | ... |  675.0  |
 *  |      ch1       | 30.0  | 50.0  | ... |  20.0   |
 *  |      .         |   .   |   .   | ... |    .    |
 *  |      .         |   .   |   .   | ... |    .    |
 *  |      .         |   .   |   .   | ... |    .    |
 *  |      chM-1     | 900.0 |1050.0 | ... |  105.0  |
 *
 */

template <typename Tin, typename Tout> class TypeConversion final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor (Tin); @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor (Tout); @ref setAddr rebinds its pBuf */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_typeConversion kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_typeConversion node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_typeConversion kernel. The kernel
    *          checks the input/output type pair and that both buffers have the same width and height.
    *
    * @param [in] pInputs      : Input buffer descriptors; exactly one buffer of type @c Tin. Its @c pBuf is the
    *                            initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs     : Output buffer descriptors; exactly one buffer of type @c Tout with the same
    *                            layout, width, and height as the input.
    * @param [in] pConfig : Construction-time parameters; required.
    * @param [in] nodeName     : Name of the node
    * @param [in] nodeId       : ID of the node
    * @param [in] errorCtx     : Error context for reporting constructor errors
    *
    * @throw None
    */
   TypeConversion(const TISP_BufDescList          *pInputs,
                  const TISP_BufDescList          *pOutputs,
                  const TypeConversionConfig *pConfig,
                  const char                      *nodeName,
                  uint32_t                         nodeId,
                  ErrorCtxt                       &errorCtx);

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
    * @retval    size_t : The number of input buffers (1: input buffer)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: output buffer)
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

   ~TypeConversion() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
