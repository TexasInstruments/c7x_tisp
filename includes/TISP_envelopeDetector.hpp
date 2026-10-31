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
 * \class EnvelopeDetector
 * @brief Provides node-level APIs for the AUDIOLIB_envelopeDetector kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details Follows the magnitude of every channel and writes its envelope: a rising |x| is tracked with the
 *          @c attackTime constant and a falling one with the @c releaseTime constant; the envelope carries across
 *          frames. Time constants are updatable via @ref setParams without resetting the envelope. Input and
 *          output are non-interleaved (one channel per row), hold at most 64 channels and must be different
 *          buffers.
 **/

/**
 * <b>Runtime Control Philosophy:</b>
 * The EnvelopeDetector node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class EnvelopeDetectorParamId : uint16_t {
   kAttackTime  = 0, /**< kAttackTime: Attack time constant in seconds */
   kReleaseTime = 1, /**< kReleaseTime: Release time constant in seconds */
   kAll         = 2  /**< kAll: all tunable params as one @ref EnvelopeDetectorTunableParams payload */
};

/**
 * @brief Construction-time (non-tunable) parameters of the @ref EnvelopeDetector node.
 *
 * @details Required.
 */
struct EnvelopeDetectorConfig {
   int32_t samplingRate = 48000; /*!< Sampling rate of the input in Hz */
};

/**
 * @brief Tunable parameters of the @ref EnvelopeDetector node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref EnvelopeDetectorParamId::kAll payload of
 *          @ref EnvelopeDetector::setParams / @ref EnvelopeDetector::getParams.
 *
 * @tparam T : Node element type (float or double).
 */
template <typename T> struct EnvelopeDetectorTunableParams {
   float attackTime  = 0.002f; /*!< Attack time constant in seconds */
   float releaseTime = 0.1f;   /*!< Release time constant in seconds */
};

template <typename T> class EnvelopeDetector final : public VirtualOp {

   NodeInfo                         _nodeInfo;      /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D                   _in;            /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D                   _out;           /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   EnvelopeDetectorConfig           _config;        /*!< Construction-time parameters */
   EnvelopeDetectorTunableParams<T> _tunableParams; /*!< Current tunable parameters */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_envelopeDetector kernel */

 public:
   /**
    * @brief Creates an AUDIOLIB_envelopeDetector node.
    *
    * @details Validates the buffer descriptors, initializes the AUDIOLIB_envelopeDetector kernel and loads the
    *          initial time constants into it.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one non-interleaved buffer. Its @c pBuf is
    *                                    the initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one non-interleaved buffer with the
    *                                    dimensions of the input.
    * @param [in] pConfig          : Construction-time parameters; required.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   EnvelopeDetector(const TISP_BufDescList                 *pInputs,
                    const TISP_BufDescList                 *pOutputs,
                    const EnvelopeDetectorConfig           *pConfig,
                    const EnvelopeDetectorTunableParams<T> *pDefaultTunableParams,
                    const char                             *nodeName,
                    uint32_t                                nodeId,
                    ErrorCtxt                              &errorCtx);

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
    * @brief Update runtime parameters with detailed error reporting
    *
    * Updates the specified runtime parameter by ID with the data provided in the value buffer.
    * The node strictly validates that the provided `size` argument matches or exceeds the
    * expected byte size for the given parameter before mapping the state into the active node memory.
    *
    * @param [in] paramId      : Parameter ID to update
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
    * This API fetches the current live state from the node for the specified parameter ID
    * and writes it into the provided buffer.
    *
    * If the user provides a buffer, it populates the requested data. If the buffer is too small
    * (or `value` is `nullptr`), it safely fails (returns `false`) but injects the exact required
    * byte capacity into `*size`, allowing the application to re-allocate precisely.
    *
    * @param [in] paramId      : Parameter ID to query
    * @param [out] value       : Pointer to a buffer where parameters will be stored
    * @param [in,out] size     : Input: Capacity of value buffer in bytes. Output: Actual bytes written or required.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true          : Parameters retrieved successfully
    * @retval false         : Failed to retrieve parameters
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

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
   ~EnvelopeDetector() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
