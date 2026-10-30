// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <cstdint>

namespace TISP {

namespace DSPLIB {

/**
 *  @ingroup dsplibNode
 */

/**@{*/

/**
 *  \class CascadeBiquad
 *  @brief Provides node-level APIs for the DSPLIB_CascadeBiquad kernel from DSPLIB.
 *  @tparam T        : Supports float and double
 *
 *  @details Applies a cascade of biquad sections to each channel of an interleaved input block.
 *           The state buffer carries the per-channel filter variables across blocks.
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The CascadeBiquad node supports dynamic parameter updates via a robust runtime API. Users can query
 * or modify specific algorithmic parameters (such as filter coefficients) without needing to
 * reconstruct the node.
 *
 */
enum class CascadeBiquadParamId : uint16_t {
   kBypass       = 0, /**< kBypass: uint32_t bypass mode; `0` applies the filter, `1` copies input to output */
   kFilterCoeffs = 1, /**< kFilterCoeffs: Filter coefficients block, in the layout of the coefficient buffer */
   kAll          = 2  /**< kAll: all tunable params as one @ref CascadeBiquadTunableParams payload */
};

/**
 * @brief Construction-time (non-tunable) parameters of the @ref CascadeBiquad node.
 *
 * @details Required; selects the filter structure, which also sets the layout of the coefficient buffer
 *          (see DSPLIB_cascadeBiquad_get_coefficientsBufferDesc).
 */
struct CascadeBiquadConfig {
   size_t   numStages       = 0U;    /*!< Number of biquad stages. */
   uint32_t biquadForm      = 0U;    /*!< Biquad form (DSPLIB_BIQUAD_DF1, DSPLIB_BIQUAD_DF2, ...). */
   bool     enableNChCoeffs = false; /*!< `true`: one coefficient set per channel, `false`: one set for all. */
};

/**
 * @brief Tunable parameters of the @ref CascadeBiquad node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref CascadeBiquadParamId::kAll payload of
 *          @ref CascadeBiquad::setParams / @ref CascadeBiquad::getParams.
 *
 * @tparam T : Node element type (float or double).
 */
template <typename T> struct CascadeBiquadTunableParams {
   T       *pCoeffs    = nullptr; /*!< Caller-owned coefficient buffer in kernel layout; required, must outlive
                                       the node. */
   uint32_t bypassMode = 0U;      /*!< `0`: apply the filter, `1`: copy input to output. */
};

template <typename T> class CascadeBiquad final : public VirtualOp {

   NodeInfo       _nodeInfo;       /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;             /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;            /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _state;          /*!< State buffer descriptor (filter variables) */
   T             *_coeffs;         /*!< Caller-owned coefficient buffer, used live by exec and setParams */
   size_t         _coeffSizeBytes; /*!< Size in bytes of the coefficient buffer */
   uint32_t       _bypassMode;     /*!< Bypass mode: `0` applies the filter, `1` copies input to output */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level DSPLIB_cascadeBiquad kernel */

   /**
    * @brief Copies the input buffer described by @ref _in to the output buffer described by @ref _out,
    *        row by row, honouring both row pitches.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

 public:
   /**
    * @brief Creates a DSPLIB_cascadeBiquad node.
    *
    * @details Validates the buffer descriptors and initializes the DSPLIB_cascadeBiquad kernel. Rows are samples
    *          and columns are channels; the state buffer is cleared.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one interleaved buffer. Its @c pBuf is the
    *                                    initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one interleaved buffer.
    * @param [in] pStates               : State buffer descriptors; exactly one buffer with one column per channel and
    *                                    `numStages` x (state variables per stage) rows.
    * @param [in] pConfig          : Construction-time parameters; required.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with a non-null @c pCoeffs.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   CascadeBiquad(const TISP_BufDescList              *pInputs,
                 const TISP_BufDescList              *pOutputs,
                 const TISP_BufDescList              *pStates,
                 const CascadeBiquadConfig      *pConfig,
                 const CascadeBiquadTunableParams<T> *pDefaultTunableParams,
                 const char                          *nodeName,
                 uint32_t                             nodeId,
                 ErrorCtxt                           &errorCtx);

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
    * @brief Clean and reset all internal state buffers to zero
    *
    * @retval None
    * @throw None
    */
   void cleanInternalBuffers() noexcept override;

   /**
    * @brief Destructor
    *
    * This destructor is set to default.
    */
   ~CascadeBiquad() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
