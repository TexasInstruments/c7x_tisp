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
 * The parameters `kThreshold`, `kKneeWidth`, `kAttack`, `kRelease`, and `kBypass` can be changed at runtime.
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
 *    T        *releaseCoeff; // Array of release coeff values, one per channel
 *                            // (length: _numChannels, Range: [0.0, 1.0])
 * } LimiterReleasePayload;
 *
 * // 5. Bypass Payload (LimiterParamId::kBypass)
 * typedef struct LimiterBypassPayload
 * {
 *    uint16_t paramId;    // Must be LimiterParamId::kBypass (4)
 *    uint32_t bypassMode; // 0: apply the limiter, 1: copy input to output
 * } LimiterBypassPayload;
 *
 * Setting kBypass back to 0 discards the gain-smoothing state, because it stopped tracking the
 * signal while bypass was engaged.
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
   kRelease   = 3, /**< kRelease: Release coeff values to set/get for each channel */
   kBypass    = 4  /**< kBypass: `0` applies the limiter, `1` copies input to output */
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

   T *_pS; /*!< Pointer to address of scratch matrix */
   T *_pY; /*!< Pointer to address of result matrix*/
   T *_pGainState; /*!< Pointer to address of gain state buffer*/

   T *_pThreshold_dB; /*!< Pointer to address of threshold in db vector */
   T *_pKneeWidth_dB; /*!< Pointer to address of kneewidth in db vector */
   T *_pAttackCoeff;  /*!< Pointer to address of attack coefficient vector */
   T *_pReleaseCoeff; /*!< Pointer to address of release coefficient vector */

   size_t _numChannels; /*!< Number of channels. */
   size_t _blockSize;   /*!< Number of samples per channel per block. */

   size_t _pitchX; /*!< Pitch in bytes of the input matrix. */
   size_t _pitchS; /*!< Pitch in bytes of the scratch matrix. */
   size_t _pitchY; /*!< Pitch in bytes of the output matrix. */

   uint32_t _bypassMode; /*!< Bypass mode: `0` applies the limiter, `1` copies input to output. */

   TISP::HandlePtr<char> _handle_dB20;         /*!< handle required by AUDIOLIB_dB20 kernel */
   TISP::HandlePtr<char> _handle_undB20;       /*!< handle required by AUDIOLIB_undB20 kernel */
   TISP::HandlePtr<char> _handle_mul;          /*!< handle for the internal multiply stage */
   TISP::HandlePtr<char> _handle_abs;          /*!< handle for the internal abs stage */
   TISP::HandlePtr<char> _handle_gainComputer; /*!< handle for the internal gain-computer stage */

   // Private functions used in limiter algorithm

   /**
    * @brief Configures the SE/SA descriptors for the internal absolute-value stage
    *
    * @param[in]  handle       : Handle the SE/SA descriptors are written into
    * @param[in]  bufParamsIn  : Buffer parameters of the input matrix
    * @param[in]  bufParamsOut : Buffer parameters of the output matrix
    * @retval     int32_t      : `0` on success
    * @throw None
    */
   int32_t
   TISP_limiter_abs_init(void *handle, AUDIOLIB_bufParams2D_t *bufParamsIn, AUDIOLIB_bufParams2D_t *bufParamsOut);

   /**
    * @brief Configures the SE/SA descriptors for the internal element-wise multiply stage
    *
    * @param[in]  handle        : Handle the SE/SA descriptors are written into
    * @param[in]  bufParamsIn0  : Buffer parameters of the first input matrix
    * @param[in]  bufParamsIn1  : Buffer parameters of the second input matrix
    * @param[in]  bufParamsOut  : Buffer parameters of the output matrix
    * @retval     int32_t       : `0` on success
    * @throw None
    */
   int32_t TISP_limiter_mul_init(void                   *handle,
                                 AUDIOLIB_bufParams2D_t *bufParamsIn0,
                                 AUDIOLIB_bufParams2D_t *bufParamsIn1,
                                 AUDIOLIB_bufParams2D_t *bufParamsOut);

   /**
    * @brief Writes the absolute value of each input sample to the output matrix
    *
    * @param[in]  handle : Handle holding the SE/SA descriptors set by @ref TISP_limiter_abs_init
    * @param[in]  pIn    : Pointer to the input matrix
    * @param[out] pOut   : Pointer to the output matrix
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_limiter_abs_exec)
   inline void TISP_limiter_abs_exec(void *handle, T *pIn, T *pOut);

   /**
    * @brief Writes the element-wise product of two input matrices to the output matrix
    *
    * @param[in]  handle : Handle holding the SE/SA descriptors set by @ref TISP_limiter_mul_init
    * @param[in]  pIn0   : Pointer to the first input matrix
    * @param[in]  pIn1   : Pointer to the second input matrix
    * @param[out] pOut   : Pointer to the output matrix
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_limiter_mul_exec)
   inline void TISP_limiter_mul_exec(void *handle, T *pIn0, T *pIn1, T *pOut);

   /**
    * @brief Configures the SE/SA descriptors for the internal gain-computer stage
    *
    * @param[in]  handle        : Handle the SE/SA descriptors are written into
    * @param[in]  bufParamsIn   : Buffer parameters of the input matrix
    * @param[in]  bufParamsGain : Buffer parameters of the per-channel gain vector
    * @param[in]  bufParamsOut  : Buffer parameters of the output matrix
    * @retval     int32_t       : `0` on success
    * @throw None
    */
   int32_t TISP_limiter_gainComputer_init(void                   *handle,
                                          AUDIOLIB_bufParams2D_t *bufParamsIn,
                                          AUDIOLIB_bufParams1D_t *bufParamsGain,
                                          AUDIOLIB_bufParams2D_t *bufParamsOut);

   /**
    * @brief Computes the per-sample limiter gain from the dB-domain input
    *
    * Applies the knee policy and the attack/release one-pole smoothing, writing the smoothed
    * gain to the output matrix.
    *
    * @param[in]  handle : Handle holding the SE/SA descriptors set by @ref TISP_limiter_gainComputer_init
    * @param[in]  pIn    : Pointer to the dB-domain input matrix
    * @param[out] pOut   : Pointer to the output matrix
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_limiter_gainComputer_exec)
   inline void TISP_limiter_gainComputer_exec(void *handle, T *pIn, T *pOut);

   /**
    * @brief Computes the vector reciprocal used by the gain computer
    *
    * @tparam     vecType : C7x vector type of the operand
    * @param[in]  vIn     : Input vector
    * @retval     vecType : Element-wise reciprocal of @p vIn
    * @throw None
    */
   __INLINE_FUNC(TISP_limiter_div_exec)
   template <typename vecType> inline vecType TISP_limiter_div_exec(vecType vIn);

   /**
    * @brief Copies the input matrix to the output matrix, honouring @ref _pitchX and @ref _pitchY
    *
    * Used when @ref _bypassMode is non-zero. The copied geometry follows
    * @ref _numChannels columns by @ref _blockSize rows.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

 public:
   /**
    * @brief Creates a AUDIOLIB_limiter node
    *
    * This constructor is called to create the node
    *
    * @param[in]  *pX      : Pointer to input matrix; initializes @ref _pX
    * @param[in]  *pS      : Pointer to scratch matrix; initializes @ref _pS
    * @param[out] *pY      : Pointer to output matrix; initializes @ref _pY
    * @param[in,out] *pGainState : Per-channel gain-smoothing state (length: numChannels). Initializes @ref _pGainState.

    * @param[in]  *pThreshold_dB      : Pointer to threshold vector; initializes @ref _pThreshold_dB
    * @param[in]  *pKneeWidth_dB      : Pointer to kneewidth vector; initializes @ref _pKneeWidth_dB
    * @param[in]  *pAttackCoeff      : Pointer to attack coefficient vector; initializes @ref _pAttackCoeff
    * @param[in]  *pReleaseCoeff      : Pointer to release coefficient vector; initializes @ref _pReleaseCoeff

    * @param[in]  numChannels    : number of channels of input matrix
    * @param[in]  blockSize   : block size of input matrix
    * @param[in]  inPitchBytes  : Pitch of input matrix in bytes
    * @param[in]  pitchS  : Pitch of scratch matrix in bytes
    * @param[in]  outPitchBytes : Pitch of output matrix in bytes
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    *
    */
   Limiter(T         *pX,
           T         *pS,
           T         *pY,
           T         *pGainState,
           T         *pThreshold_dB,
           T         *pKneeWidth_dB,
           T         *pAttackCoeff,
           T         *pReleaseCoeff,
           size_t     numChannels,
           size_t     blockSize,
           size_t     inPitchBytes,
           size_t     pitchS,
           size_t     outPitchBytes,
           ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_limiter node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Limiter(T          *pX,
           T          *pS,
           T          *pY,
           T          *pGainState,
           T          *pThreshold_dB,
           T          *pKneeWidth_dB,
           T          *pAttackCoeff,
           T          *pReleaseCoeff,
           size_t      numChannels,
           size_t      blockSize,
           size_t      inPitchBytes,
           size_t      pitchS,
           size_t      outPitchBytes,
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
    * @param[in] addrs : Structure containing source and destination addresses
    * @param[in] mode : Mode of address setting (input, output, or both)
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
    * @brief Performance estimation of module
    *
    * This function performs an estimation of the total cycle counts of the module.
    *
    */
   /* void perfEst(size_t *estCycles, size_t *archCycles) noexcept; */

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
    * @brief Clean and reset all internal state and history buffers to zero
    *
    * @details Zeroes the per-channel gain-smoothing state in @ref _pGainState, which is the
    *          node's only internal state. Called by the constructor once initialization has
    *          succeeded, so the envelope always starts from rest, and by @ref setParams when
    *          @ref LimiterParamId::kBypass returns from `1` to `0`, since the envelope froze
    *          while bypassed. Call it directly to restart a stream without reconstructing the
    *          node.
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

   ~Limiter() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
