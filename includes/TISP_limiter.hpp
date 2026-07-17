// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <audiolib.h>

namespace TISP {

namespace AUDIOLIB {

/**
 * <b>Runtime Control:</b>
 * The parameters `kThreshold`, `kKneeWidth`, `kAttack`, and `kRelease` can be changed at runtime.
 * To update parameters, use `setParams()` with the payload format specified below.
 * Use `getParams()` to retrieve the current parameters.
 *
 * <b>Payload Format for setParams() and getParams():</b>
 * @code
 * // 1. Threshold Payload (LimiterParamId::kThreshold)
 * typedef struct LimiterThresholdPayload
 * {
 *    uint16_t paramId;       // Must be LimiterParamId::kThreshold (0)
 *    T        *threshold_dB; // Array of threshold values, one per channel (length: _numChannels, Range: <= 0.0)
 * } LimiterThresholdPayload;
 *
 * // 2. Knee Width Payload (LimiterParamId::kKneeWidth)
 * typedef struct LimiterKneeWidthPayload
 * {
 *    uint16_t paramId;       // Must be LimiterParamId::kKneeWidth (1)
 *    T        *kneeWidth_dB; // Array of knee width values, one per channel (length: _numChannels, Range: >= 0.0)
 * } LimiterKneeWidthPayload;
 *
 * // 3. Attack Coefficient Payload (LimiterParamId::kAttack)
 * typedef struct LimiterAttackPayload
 * {
 *    uint16_t paramId;       // Must be LimiterParamId::kAttack (2)
 *    T        *attackCoeff;  // Array of attack coeff values, one per channel (length: _numChannels, Range: [0.0, 1.0])
 * } LimiterAttackPayload;
 *
 * // 4. Release Coefficient Payload (LimiterParamId::kRelease)
 * typedef struct LimiterReleasePayload
 * {
 *    uint16_t paramId;       // Must be LimiterParamId::kRelease (3)
 *    T        *releaseCoeff; // Array of release coeff values, one per channel (length: _numChannels, Range:
 * [0.0, 1.0]) } LimiterReleasePayload;
 * @endcode
 */
/**
 * <b>Runtime Control Philosophy:</b>
 * The Limiter node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class LimiterParamId : uint16_t {
   kThreshold = 0, /**< kThreshold: Threshold values to set/get for each channel */
   kKneeWidth = 1, /**< kKneeWidth: Knee width values to set/get for each channel */
   kAttack    = 2, /**< kAttack: Attack coeff values to set/get for each channel */
   kRelease   = 3  /**< kRelease: Release coeff values to set/get for each channel */
};

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 *  \class Limiter
 *  @brief Provides node-level APIs for limiter algorithm using AUDIOLIB kernels.
 *  @tparam T  : Supports float
 *
 *  @details The algorithm prevents a signal from exceeding a specified
 *  threshold by limiting the dynamic range of the signal. The limited
 *  region is specified by the knee policy. For an input of 0 dB, a
 *  corresponding make-up gain will be applied to maintain unity gain.
 *  This computed gain is post-processed with a one-pole filter with
 *  user-provided attack/release coefficients for additional gain smoothing.
 *
 *  All buffers are expected to be in interleaved format, i.e. each
 *  column contains an independent channel of sample sequences. The tunable
 *  parameter buffers will be independently applied to each channel. For best
 *  performance, user should provide a channel count at least the size of
 *  SIMD width.
 *
 */

template <typename T> class Limiter final : public VirtualOp {

   /* private members */

   T       *_pX;       /*!< Pointer to input buffer */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pG; /*!< Pointer to address of gain vector */
   T *_pS; /*!< Pointer to address of scratch matrix */
   T *_pY; /*!< Pointer to address of result matrix*/

   T *_pThreshold_dB; /*!< Pointer to address of threshold in db vector */
   T *_pKneeWidth_dB; /*!< Pointer to address of kneewidth in db vector */
   T *_pAttackCoeff;  /*!< Pointer to address of attack coefficient vector */
   T *_pReleaseCoeff; /*!< Pointer to address of release coefficient vector */

   size_t _numChannels; /*!< Number of channels. */
   size_t _blockSize;   /*!< Number of samples per channel per block. */

   size_t _pitchX; /*!< Pitch in bytes of the input matrix. */
   size_t _pitchS; /*!< Pitch in bytes of the scratch matrix. */
   size_t _pitchY; /*!< Pitch in bytes of the output matrix. */

   bool _isInterleaved; /*!< Input/output layout: true = interleaved, false = non-interleaved. */

   TISP::HandlePtr<char> _handle_dB20;        /*!< handle required by AUDIOLIB_dB20 kernel */
   TISP::HandlePtr<char> _handle_undB20;      /*!< handle required by AUDIOLIB_undB20 kernel */
   TISP::HandlePtr<char> _handle_mul;         /*!< handle for the internal multiply stage */
   TISP::HandlePtr<char> _handle_abs;         /*!< handle for the internal abs stage */
   TISP::HandlePtr<char> _handle_gainComputer; /*!< handle for the internal gain-computer stage */

   // Private functions used in limiter algorithm
   int32_t
   TISP_limiter_abs_init(void *handle, AUDIOLIB_bufParams2D_t *bufParamsIn, AUDIOLIB_bufParams2D_t *bufParamsOut);

   int32_t TISP_limiter_mul_init(void                   *handle,
                                 AUDIOLIB_bufParams2D_t *bufParamsIn0,
                                 AUDIOLIB_bufParams2D_t *bufParamsIn1,
                                 AUDIOLIB_bufParams2D_t *bufParamsOut);

   __INLINE_FUNC(TISP_limiter_abs_exec)
   inline void TISP_limiter_abs_exec(void *handle, T *pIn, T *pOut);

   __INLINE_FUNC(TISP_limiter_mul_exec)
   inline void TISP_limiter_mul_exec(void *handle, T *pIn0, T *pIn1, T *pOut);

   int32_t TISP_limiter_gainComputer_init(void                   *handle,
                                          AUDIOLIB_bufParams2D_t *bufParamsIn,
                                          AUDIOLIB_bufParams1D_t *bufParamsGain,
                                          AUDIOLIB_bufParams2D_t *bufParamsOut);

   __INLINE_FUNC(TISP_limiter_gainComputer_exec)
   inline void TISP_limiter_gainComputer_exec(void *handle, T *pIn, T *pOut, T *pGain);

   __INLINE_FUNC(TISP_limiter_div_exec)
   template <typename vecType> inline vecType TISP_limiter_div_exec(vecType vIn);

 public:
   /**
    * @brief Creates a AUDIOLIB_limiter node
    *
    * This constructor is called to create the node
    *
    * @param[in]  *pX      : Pointer to input matrix; initializes @ref _pX
    * @param[out]  *pG      : Pointer to gain vector; initializes @ref _pG
    * @param[in]  *pS      : Pointer to scratch matrix; initializes @ref _pS
    * @param[out] *pY      : Pointer to output matrix; initializes @ref _pY

    * @param[in]  *pThreshold_dB      : Pointer to threshold vector; initializes @ref _pThreshold_dB
    * @param[in]  *pKneeWidth_dB      : Pointer to kneewidth vector; initializes @ref _pKneeWidth_dB
    * @param[in]  *pAttackCoeff      : Pointer to attack coefficient vector; initializes @ref _pAttackCoeff
    * @param[in]  *pReleaseCoeff      : Pointer to release coefficient vector; initializes @ref _pReleaseCoeff

    * @param[in]  numChannels    : number of channels of input matrix
    * @param[in]  blockSize   : block size of input matrix
    * @param[in]  pitchX  : Pitch of input matrix in bytes
    * @param[in]  pitchS  : Pitch of scratch matrix in bytes
    * @param[in]  pitchY : Pitch of output matrix in bytes
    * @param[in]  isInterleaved : Input data is interleaved or not.
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   Limiter(T         *pX,
           T         *pG,
           T         *pS,
           T         *pY,
           T         *pThreshold_dB,
           T         *pKneeWidth_dB,
           T         *pAttackCoeff,
           T         *pReleaseCoeff,
           size_t     numChannels,
           size_t     blockSize,
           size_t     pitchX,
           size_t     pitchS,
           size_t     pitchY,
           bool       isInterleaved, /* Default is set to interleaved data format */
           ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_limiter node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param[in]  *pX           : Pointer to input matrix; initializes @ref _pX
    * @param[out] *pG           : Pointer to gain vector; initializes @ref _pG
    * @param[in]  *pS           : Pointer to scratch matrix; initializes @ref _pS
    * @param[out] *pY           : Pointer to output matrix; initializes @ref _pY
    * @param[in]  *pThreshold_dB : Pointer to threshold vector; initializes @ref _pThreshold_dB
    * @param[in]  *pKneeWidth_dB : Pointer to kneewidth vector; initializes @ref _pKneeWidth_dB
    * @param[in]  *pAttackCoeff  : Pointer to attack coefficient vector; initializes @ref _pAttackCoeff
    * @param[in]  *pReleaseCoeff : Pointer to release coefficient vector; initializes @ref _pReleaseCoeff
    * @param[in]  numChannels   : number of channels of input matrix
    * @param[in]  blockSize     : block size of input matrix
    * @param[in]  pitchX        : Pitch of input matrix in bytes
    * @param[in]  pitchS        : Pitch of scratch matrix in bytes
    * @param[in]  pitchY        : Pitch of output matrix in bytes
    * @param[in]  isInterleaved : Input data is interleaved or not.
    * @param[in]  nodeName      : Name of the node; sets @ref _nodeInfo name.
    * @param[in]  nodeId        : ID of the node; sets @ref _nodeInfo ID.
    * @param[in]  errorCtx      : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */
   Limiter(T          *pX,
           T          *pG,
           T          *pS,
           T          *pY,
           T          *pThreshold_dB,
           T          *pKneeWidth_dB,
           T          *pAttackCoeff,
           T          *pReleaseCoeff,
           size_t      numChannels,
           size_t      blockSize,
           size_t      pitchX,
           size_t      pitchS,
           size_t      pitchY,
           bool        isInterleaved,
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
    * @throw None
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pY (output data matrix) - managed by setAddr()
    * - index 1: _pG (gain value) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0 or 1)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    * @throw None
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _pX)
    * @throw None
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (2: _pY)
    * @throw None
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param[in] addrs : Structure containing source and destination addresses
    * @param[in] mode : Mode of address setting (input, output, or both)
    * @retval None
    * @throw None
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
    * @throw None
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
    * @throw None
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

   /**
    * @brief Execute the operation
    *
    * This function performs the actual operation.
    *
    * @retval None
    * @throw None
    */
   void exec() noexcept override;

   /**
    * @brief Performance estimation of module
    *
    * This function performs an estimation of the total cycle counts of the module.
    *
    * @retval None
    * @throw None
    */
   /* void perfEst(size_t *estCycles, size_t *archCycles) noexcept; */

   /**
    * @brief Get the name of the node
    *
    * This function returns the name of the node.
    *
    * @retval const char* The name of the node
    * @throw None
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node
    *
    * This function returns the ID of the node.
    *
    * @retval uint32_t The ID of the node
    * @throw None
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node
    *
    * This function returns a string containing the parameters of the node.
    *
    * @retval const char* The parameters of the node
    * @throw None
    */
   const char *getNodeParams() const noexcept override { return _nodeInfo.getNodeParams(); }

   /**
    * @brief Get the input and output buffer parameters of the node
    *
    * This function returns reference to node's i/o buffer properties
    *
    * @retval const NodeBufParams& : A constant reference to the NodeBufParams representing node's i/o buffer properties
    * @throw None
    */
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    * @retval None
    * @throw None
    */

   ~Limiter() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
