// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <array>

namespace TISP {

namespace DSPLIB {

/**
 *  @ingroup dsplibNode
 */

/**@{*/

/**
 * @brief Runtime parameter IDs of the @ref BinaryLogicGate node for setParams / getParams.
 */
enum class BinaryLogicGateParamId : uint16_t {
   kOpType = 0, /**< kOpType: Bitwise operator applied to every element pair */
   kAll    = 1  /**< kAll: all tunable params as one @ref BinaryLogicGateTunableParams payload */
};

/**
 * @brief Tunable parameters of the @ref BinaryLogicGate node, with their default values.
 *
 * @details Initial values of the runtime parameters and the @ref BinaryLogicGateParamId::kAll payload; required.
 *
 * @tparam T : Node element type.
 */
template <typename T> struct BinaryLogicGateTunableParams {
   uint32_t opType = 0U; /*!< Bitwise operator applied to every element pair (0 = AND, 1 = OR, 2 = XOR) */
};

/**
 * \class BinaryLogicGate
 * @brief Provides node-level APIs for the DSPLIB_binaryLogicGate kernel from DSPLIB.
 * @tparam T : Supports uint8_t, uint16_t, uint32_t and uint64_t.
 *
 * @details Applies the bitwise operator selected by @ref BinaryLogicGateTunableParams::opType to every pair of
 *          elements of the two inputs and writes the result. Both inputs and the output share one layout
 *          (interleaved or non-interleaved), width and height; the two inputs share one row pitch, the output pitch
 *          may differ. The operator is updatable via @ref setParams.
 *
 *  <b>Bitwise operators (@ref BinaryLogicGateTunableParams::opType): </b>
 *
 *  | opType | Operation |
 *  |--------|-----------|
 *  |   0    |    AND    |
 *  |   1    |    OR     |
 *  |   2    |    XOR    |
 *
 * <b>Example (AND): </b>
 *
 * <b>Input 0</b>: width = 3; height = 2; T = uint8_t.
 *  | Row/Column | c0   | c1   | c2   |
 *  |------------|------|------|------|
 *  |     r0     | 0xF0 | 0x0F | 0xFF |
 *  |     r1     | 0xAA | 0x55 | 0x00 |
 *
 * <b>Input 1</b>: width = 3; height = 2.
 *  | Row/Column | c0   | c1   | c2   |
 *  |------------|------|------|------|
 *  |     r0     | 0xFF | 0xFF | 0x0F |
 *  |     r1     | 0x0F | 0x0F | 0xFF |
 *
 * <b>Output</b>: width = 3; height = 2.
 *  | Row/Column | c0   | c1   | c2   |
 *  |------------|------|------|------|
 *  |     r0     | 0xF0 | 0x0F | 0x0F |
 *  |     r1     | 0x0A | 0x05 | 0x00 |
 **/

template <typename T> class BinaryLogicGate final : public VirtualOp {

   NodeInfo                       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   std::array<TISP_BufDesc2D, 2U> _in;       /*!< Input buffer descriptors */
   std::array<void *, 2U>         _inputs;   /*!< Input buffer pointers; @ref setAddr rebinds them */
   TISP_BufDesc2D                 _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   uint32_t                       _opType;   /*!< Current value of the tunable parameter opType */

   TISP::HandlePtr<char> _handle; /*!< Handle to the low level kernel called in exec() */

 public:
   /**
    * @brief Creates a BinaryLogicGate node.
    *
    * @details Validates the buffer descriptors and parameters, then initializes the node.
    *
    * @param [in] pInputs        : Input buffer descriptors; exactly 2 buffers: [0] interleaved or planar, [1]
    * interleaved or planar.
    * @param [in] pOutputs       : Output buffer descriptors; exactly 1 buffer: [0] interleaved or planar.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx       : Error context for reporting constructor errors
    *
    * @throw None
    */
   BinaryLogicGate(const TISP_BufDescList                *pInputs,
                   const TISP_BufDescList                *pOutputs,
                   const BinaryLogicGateTunableParams<T> *pDefaultTunableParams,
                   const char                            *nodeName,
                   uint32_t                               nodeId,
                   ErrorCtxt                             &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0 to 1: input buffers - managed by setAddr()
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
    * @brief Update runtime parameters with detailed error reporting
    *
    * @param [in] paramId      : Parameter ID to update (@ref BinaryLogicGateParamId)
    * @param [in] value        : Pointer to a buffer containing the new parameter value
    * @param [in] size         : The size in bytes of the payload
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true         : Parameters updated successfully
    * @retval false        : Failed to update parameters
    */
   bool setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept override;

   /**
    * @brief Retrieve current runtime parameters with detailed error reporting
    *
    * @param [in] paramId      : Parameter ID to query (@ref BinaryLogicGateParamId)
    * @param [out] value       : Pointer to a buffer where parameters will be stored
    * @param [in,out] size     : Input: capacity of value in bytes. Output: bytes written or required.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true          : Parameters retrieved successfully
    * @retval false         : Failed to retrieve parameters
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

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
   ~BinaryLogicGate() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
