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
 * @brief Runtime parameter IDs of the @ref ThresholdDetector node for setParams / getParams.
 */
enum class ThresholdDetectorParamId : uint16_t {
   kThreshold = 0, /**< kThreshold: Scalar threshold every element is compared with */
   kOpType    = 1, /**< kOpType: Relational operator */
   kAll       = 2  /**< kAll: all tunable params as one @ref ThresholdDetectorTunableParams payload */
};

/**
 * @brief Tunable parameters of the @ref ThresholdDetector node, with their default values.
 *
 * @details Initial values of the runtime parameters and the @ref ThresholdDetectorParamId::kAll payload; required.
 *
 * @tparam Tin : Node element type.
 */
template <typename Tin> struct ThresholdDetectorTunableParams {
   Tin      threshold = 0;  /*!< Scalar threshold every element is compared with */
   uint32_t opType    = 0U; /*!< Relational operator (0 = EQ, 1 = NE, 2 = LT, 3 = LE, 4 = GT, 5 = GE) */
};

/**
 * \class ThresholdDetector
 * @brief Provides node-level APIs for the DSPLIB_thresholdDetector kernel from DSPLIB.
 * @tparam Tin  : Supports int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float and double.
 * @tparam Tout : Unsigned integer type of the same size as @c Tin (see the table below).
 *
 * @details Compares every element of the input with a scalar threshold using the relational operator selected by
 *          @ref ThresholdDetectorTunableParams::opType and writes `1` where the comparison holds and `0` where it does
 *          not. The threshold (@ref ThresholdDetectorTunableParams::threshold) and the operator are updatable via @ref
 *setParams. Input and output share one layout, width and height; their row pitches may differ.
 *
 *  <b>Type Combinations: </b>
 *
 *  |    Input data type (Tin)   |   Output data type (Tout)  |
 *  |----------------------------|----------------------------|
 *  |     int8_t, uint8_t        |          uint8_t           |
 *  |     int16_t, uint16_t      |          uint16_t          |
 *  |     int32_t, uint32_t      |          uint32_t          |
 *  |     int64_t, uint64_t      |          uint64_t          |
 *  |          float             |          uint32_t          |
 *  |          double            |          uint64_t          |
 *
 *  <b>Relational operators (@ref ThresholdDetectorTunableParams::opType): </b>
 *
 *  | opType | Operation                         |
 *  |--------|-----------------------------------|
 *  |   0    | input == threshold                 |
 *  |   1    | input != threshold                 |
 *  |   2    | input <  threshold                 |
 *  |   3    | input <= threshold                 |
 *  |   4    | input >  threshold                 |
 *  |   5    | input >= threshold                 |
 *
 * <b>Example: </b>
 *
 * <b>Input</b>: width = 3; height = 3; Tin = int32_t, Tout = uint32_t.
 *  | Row/Column | c0  | c1  | c2  |
 *  |------------|-----|-----|-----|
 *  |     r0     | 23  | 146 | 179 |
 *  |     r1     | 57  | 131 | 29  |
 *  |     r2     | 38  | 43  | 88  |
 *
 * <b>Constant</b> (@ref ThresholdDetectorTunableParams::threshold): 64 <br>
 * <b>opType</b> (@ref ThresholdDetectorTunableParams::opType): 3 (less than or equal) <br>
 *
 * <b>Output</b>: width = 3; height = 3.
 *  | Row/Column | c0 | c1 | c2 |
 *  |------------|----|----|----|
 *  |     r0     | 1  | 0  | 0  |
 *  |     r1     | 1  | 0  | 1  |
 *  |     r2     | 1  | 1  | 0  |
 *
 * <b>Configuration & Parameters:</b>
 *
 * | Parameter | Configuration Time | Description                                                       |
 * |-----------|--------------------|-------------------------------------------------------------------|
 * | opType    | Runtime            | Relational operator, `0` to `5` (table above).                    |
 * | threshold | Runtime            | Value every input element is compared with, of type @c Tin.      |
 *
 **/

template <typename Tin, typename Tout> class ThresholdDetector final : public VirtualOp {

   NodeInfo       _nodeInfo;  /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;        /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;       /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   Tin            _threshold; /*!< Current value of the tunable parameter threshold */
   uint32_t       _opType;    /*!< Current value of the tunable parameter opType */

   TISP::HandlePtr<char> _handle; /*!< Handle to the low level kernel called in exec() */

 public:
   /**
    * @brief Creates a ThresholdDetector node.
    *
    * @details Validates the buffer descriptors and parameters, then initializes the node.
    *
    * @param [in] pInputs        : Input buffer descriptors; exactly 1 buffer: [0] interleaved or planar.
    * @param [in] pOutputs       : Output buffer descriptors; exactly 1 buffer: [0] interleaved or planar.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx       : Error context for reporting constructor errors
    *
    * @throw None
    */
   ThresholdDetector(const TISP_BufDescList                    *pInputs,
                     const TISP_BufDescList                    *pOutputs,
                     const ThresholdDetectorTunableParams<Tin> *pDefaultTunableParams,
                     const char                                *nodeName,
                     uint32_t                                   nodeId,
                     ErrorCtxt                                 &errorCtx);

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
    * @param [in] paramId      : Parameter ID to update (@ref ThresholdDetectorParamId)
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
    * @param [in] paramId      : Parameter ID to query (@ref ThresholdDetectorParamId)
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
   ~ThresholdDetector() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
