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
 * @brief Runtime parameter IDs of the @ref ConditionalReplace node for setParams / getParams.
 */
enum class ConditionalReplaceParamId : uint16_t {
   kCompareType  = 0, /**< kCompareType: Per-channel comparison: 0 EQ, 1 NE, 2 LT, 3 LE, 4 GT, 5 GE */
   kConstValue   = 1, /**< kConstValue: Per-channel constant the samples are compared with */
   kReplaceValue = 2, /**< kReplaceValue: Per-channel value written where the comparison holds */
   kAll          = 3  /**< kAll: all tunable params as one @ref ConditionalReplaceTunableParams payload */
};

/**
 * @brief Tunable parameters of the @ref ConditionalReplace node, with their default values.
 *
 * @details Initial values of the runtime parameters and the @ref ConditionalReplaceParamId::kAll payload;
 * required.
 *
 * @tparam T : Node element type.
 */
template <typename T> struct ConditionalReplaceTunableParams {
   int32_t *pCompareType =
       nullptr; /*!< Per-channel comparison: 0 EQ, 1 NE, 2 LT, 3 LE, 4 GT, 5 GE (caller-owned array) */
   int32_t *pConstValue   = nullptr; /*!< Per-channel constant the samples are compared with (caller-owned array) */
   int32_t *pReplaceValue = nullptr; /*!< Per-channel value written where the comparison holds (caller-owned array) */
};

/**
 * \class ConditionalReplace
 * @brief Provides node-level APIs for the DSPLIB_conditionalReplace kernel from DSPLIB.
 * @tparam T : Supports int32_t.
 *
 * @details Compares every sample of a channel with a per-channel constant and replaces it by a per-channel value
 *          where the comparison holds; the other samples are copied. The input and output are non-interleaved
 *          (one row per channel), share one width and height, and may have different row pitches. The three
 *          per-channel arrays are caller-owned, required, must hold one element per channel and must outlive the
 *          node (the kernel keeps their addresses); they are updatable via @ref setParams.
 *
 *  <b>Compare types (@ref ConditionalReplaceTunableParams::pCompareType): </b>
 *
 *  | compareType | Operation                  |
 *  |-------------|----------------------------|
 *  |      0      | input == constValue        |
 *  |      1      | input != constValue        |
 *  |      2      | input <  constValue        |
 *  |      3      | input <= constValue        |
 *  |      4      | input >  constValue        |
 *  |      5      | input >= constValue        |
 *
 * <b>Example: </b>
 *
 * <b>Input</b>: width = 3 (samples); height = 2 (channels).
 *  | Channel | s0 | s1 | s2 |
 *  |---------|----|----|----|
 *  |   ch0   | 5  | -2 | 9  |
 *  |   ch1   | 7  | 7  | 0  |
 *
 * <b>pCompareType</b>: `{4, 0}` (ch0: greater than, ch1: equal) <br>
 * <b>pConstValue</b>: `{3, 7}` <br>
 * <b>pReplaceValue</b>: `{100, -1}` <br>
 *
 * <b>Output</b>: width = 3; height = 2.
 *  | Channel | s0  | s1 | s2  |
 *  |---------|-----|----|-----|
 *  |   ch0   | 100 | -2 | 100 |
 *  |   ch1   | -1  | -1 | 0   |
 **/

template <typename T> class ConditionalReplace final : public VirtualOp {

   NodeInfo       _nodeInfo;     /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;           /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;          /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   int32_t       *_compareType;  /*!< Current value of the tunable parameter pCompareType */
   int32_t       *_constValue;   /*!< Current value of the tunable parameter pConstValue */
   int32_t       *_replaceValue; /*!< Current value of the tunable parameter pReplaceValue */

   TISP::HandlePtr<char> _handle; /*!< Handle to the low level kernel called in exec() */

 public:
   /**
    * @brief Creates an ConditionalReplace node.
    *
    * @details Validates the buffer descriptors and parameters, then initializes the node.
    *
    * @param [in] pInputs        : Input buffer descriptors; exactly 1 buffer: [0] planar.
    * @param [in] pOutputs       : Output buffer descriptors; exactly 1 buffer: [0] planar.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx       : Error context for reporting constructor errors
    *
    * @throw None
    */
   ConditionalReplace(const TISP_BufDescList                        *pInputs,
                           const TISP_BufDescList                        *pOutputs,
                           const ConditionalReplaceTunableParams<T> *pDefaultTunableParams,
                           const char                                    *nodeName,
                           uint32_t                                       nodeId,
                           ErrorCtxt                                     &errorCtx);

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
    * @brief Update runtime parameters with detailed error reporting
    *
    * @param [in] paramId      : Parameter ID to update (@ref ConditionalReplaceParamId)
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
    * @param [in] paramId      : Parameter ID to query (@ref ConditionalReplaceParamId)
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
   ~ConditionalReplace() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
