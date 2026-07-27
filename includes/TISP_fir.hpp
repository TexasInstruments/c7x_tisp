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
 *  \class Fir
 *  @brief Provides node-level APIs for the DSPLIB_fir kernel from DSPLIB.
 *  @tparam T        : Supports float
 *
 *  @details This node applies a finite impulse response filter to each channel of the input
 *  buffer pX and writes the filtered values to the output buffer pY. Filtering is performed
 *  block by block: each @ref exec() first copies the incoming block into the state buffer pS
 *  behind the retained `filterSize - 1` history samples, runs the kernel over the state buffer,
 *  and then shifts the tail of the state buffer back to the front so the next block sees the
 *  correct history. The filter coefficients pH are shared across channels unless
 *  `enableNchCoefs` is set, in which case one coefficient set per channel is used.
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The Fir node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters (such as the active filter coefficients) without needing
 * to reconstruct the node.
 *
 */
enum class FirParamId : uint16_t {
   kFilterCoeffs = 0, /**< kFilterCoeffs: Filter coefficients to set/get for the node */
   kBypass       = 1  /**< kBypass: uint32_t bypass mode; `0` applies the filter, `1` copies input to output */
};

template <typename T> class Fir final : public VirtualOp {

   /* private members */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pX; /*!< Pointer to address of input vector */
   T *_pS; /*!< Pointer to address of state vector */
   T *_pH; /*!< Pointer to address of filter vector */
   T *_pY; /*!< Pointer to address of result vector*/

   size_t _inputSize;  /*!< Input size */
   size_t _filterSize; /*!< Filter size */
   size_t _batchSize;  /*!< Number of channels */
   size_t _stateSize;  /*!< State size */

   size_t _pitchX; /*!< Input pitch */
   size_t _pitchS; /*!< State pitch */
   size_t _pitchH; /*!< Filter pitch */
   size_t _pitchY; /*!< Output pitch */

   bool   _enableNchCoefs; /*!< Enable different coeffs per channel */
   bool   _enableMMA;      /*!< Enable MMA computation */
   bool   _enableQ;        /*!< Enable fixed point Q for MMA-based computation */
   size_t _Q;              /*!< Q value */

   // input to state
   __SE_TEMPLATE_v1 _se0ParamsPre = __gen_SE_TEMPLATE_v1(); /*!< SE params for the input-to-state block copy */
   __SA_TEMPLATE_v1 _sa0ParamsPre = __gen_SA_TEMPLATE_v1(); /*!< SA params for the input-to-state block copy */

   // state delay after FIR block processing
   __SE_TEMPLATE_v1 _se0ParamsPost = __gen_SE_TEMPLATE_v1(); /*!< SE params for the state delay-line block copy */
   __SA_TEMPLATE_v1 _sa0ParamsPost = __gen_SA_TEMPLATE_v1(); /*!< SA params for the state delay-line block copy */

   uint32_t _bypassMode; /*!< Bypass mode: `0` applies the filter, `1` copies input to output. */

   TISP::HandlePtr<char> _handle; /*!< handle required by DSPLIB_fir kernel */

   /**
    * @brief Private TISP_fir block copy
    *
    * This function is called to process block copy transactions of internal buffers
    *
    * @param[in]  pX        : Pointer to input matrix
    * @param[out] pY        : Pointer to output matrix
    * @param[in]  width     : Width of the copy dimensions
    * @param[in]  height    : Height of the copy dimensions
    * @param[in]  se0Params : SE0 parameters for 2d block copy
    * @param[in]  sa0Params : SA0 parameters for 2d block copy
    * @retval None
    * @throw None
    */

   void TISP_fir_blockCopy2D(T               *pX,
                             T               *pY,
                             size_t           width,
                             size_t           height,
                             __SE_TEMPLATE_v1 se0Params,
                             __SA_TEMPLATE_v1 sa0Params);

   /**
    * @brief Copies @ref _batchSize rows of @ref _inputSize elements from @ref _pX to @ref _pY,
    *        honouring @ref _pitchX and @ref _pitchY
    *
    * Used by the bypass path. The state buffer @ref _pS is not advanced while bypass is enabled,
    * so @ref cleanInternalBuffers() is invoked when bypass is disengaged to discard the
    * now-stale filter history.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

 public:
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
    * @brief Creates a DSPLIB_fir node
    *
    * This constructor is called to create the node
    *
    * @param[in]  *pX      : Pointer to input matrix; initializes @ref _pX
    * @param[in]  *pS      : Pointer to state matrix; initializes @ref _pS
    * @param[in]  *pH      : Pointer to filter matrix; initializes @ref _pH
    * @param[out] *pY      : Pointer to output matrix; initializes @ref _pY
    * @param[in]  inputSize    : Size of input per channel
    * @param[in]  filterSize    : Size of filter per channel
    * @param[in]  stateSize   : Size of state buffer per channel
    * @param[in]  batchSize   : Number of channels
    * @param[in]  pitchX  : Pitch of input data (in bytes)
    * @param[in]  pitchS  : Pitch of state data (in bytes)
    * @param[in]  pitchH  : Pitch of filter data (in bytes)
    * @param[in]  pitchY  : Pitch of output data (in bytes)
    * @param[in]  enableNchCoefs : Flag for different coefficients per channel
    * @param[in]  enableMMA      : Flag for enabling MMA-based computation
    * @param[in]  enableQ        : Flag for enabling fixed Q format for MMA-based computation (valid only for SOCs with
    * Fixed-Point MMA)
    * @param[in]  Q              : Q value for Q-enabled computations
    * @param[in]  errorCtx       : Error context for reporting constructor errors
    *
    */

   Fir(T         *pX,
       T         *pS,
       T         *pH,
       T         *pY,
       size_t     inputSize,
       size_t     filterSize,
       size_t     stateSize,
       size_t     batchSize,
       size_t     pitchX,
       size_t     pitchS,
       size_t     pitchH,
       size_t     pitchY,
       bool       enableNchCoefs,
       bool       enableMMA, /* Default method is C7x unless specified by user */
       bool       enableQ,
       size_t     Q,
       ErrorCtxt &errorCtx);

   /**
    * @brief Creates a Fir node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Fir(T          *pX,
       T          *pS,
       T          *pH,
       T          *pY,
       size_t      inputSize,
       size_t      filterSize,
       size_t      stateSize,
       size_t      batchSize,
       size_t      pitchX,
       size_t      pitchS,
       size_t      pitchH,
       size_t      pitchY,
       bool        enableNchCoefs,
       bool        enableMMA,
       bool        enableQ,
       size_t      Q,
       const char *nodeName,
       uint32_t    nodeId,
       ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pX (input data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pY (output data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _pX)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pY)
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param[in] addrs Structure containing source and destination addresses
    * @param[in] mode Mode of address setting (input, output, or both)
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
    * @brief Clean and reset all internal state buffers to zero
    *
    * @retval None
    * @throw None
    */
   void cleanInternalBuffers() noexcept override;

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~Fir() = default;

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
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
