// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <audiolib.h>

// <mathlib.h> pulls in the compiler's c6x_migration.h, which #undefs __PRAGMA and __INLINE_FUNC at
// its end. AUDIOLIB_types.h is include-guarded and will not restore them, so redefine them here
// (same tokens as AUDIOLIB/FFTLIB) when this header is included after <mathlib.h>.
#ifndef __PRAGMA
#define __PRAGMA(x) _Pragma(#x)
#endif
#ifndef __INLINE_FUNC
#define __INLINE_FUNC(x) __PRAGMA(FUNC_ALWAYS_INLINE)
#endif

namespace TISP {

namespace AUDIOLIB {

/**
 * <b>Runtime Control:</b>
 * The parameters `kBypassMode`, `kThresholdDb`, `kRatio`, `kKneeWidthDb`, `kAttackCoeff`, `kReleaseCoeff`, `kHoldSamples`, and
 * `kAll` can be changed at runtime. To update parameters, use `setParams()` with the payload format specified below.
 * Use `getParams()` to retrieve the current parameters.
 *
 * <b>Payload Format for setParams() and getParams():</b>
 * @code
 * // 1. Bypass Payload (DownwardExpanderParamId::kBypassMode)
 * typedef struct DownwardExpanderBypassPayload
 * {
 *    uint16_t paramId;    // Must be DownwardExpanderParamId::kBypassMode (0)
 *    uint32_t bypassMode; // 0: apply the expander, 1: copy input to output
 * } DownwardExpanderBypassPayload;
 *
 * // 2. Threshold Payload (DownwardExpanderParamId::kThresholdDb)
 * typedef struct DownwardExpanderThresholdPayload
 * {
 *    uint16_t paramId;       // Must be DownwardExpanderParamId::kThresholdDb (1)
 *    T        *threshold_dB; // Array of threshold values, one per channel (length: _numChannels, Range: <= 0.0)
 * } DownwardExpanderThresholdPayload;
 *
 * // 3. Ratio Payload (DownwardExpanderParamId::kRatio)
 * typedef struct DownwardExpanderRatioPayload
 * {
 *    uint16_t paramId; // Must be DownwardExpanderParamId::kRatio (2)
 *    T        *ratio;  // Array of expansion ratios, one per channel (length: _numChannels, Range: >= 1.0)
 * } DownwardExpanderRatioPayload;
 *
 * // 4. Knee Width Payload (DownwardExpanderParamId::kKneeWidthDb)
 * typedef struct DownwardExpanderKneeWidthPayload
 * {
 *    uint16_t paramId;       // Must be DownwardExpanderParamId::kKneeWidthDb (3)
 *    T        *kneeWidth_dB; // Array of knee width values, one per channel (length: _numChannels, Range: >= 0.0)
 * } DownwardExpanderKneeWidthPayload;
 *
 * // 5. Attack Coefficient Payload (DownwardExpanderParamId::kAttackCoeff)
 * typedef struct DownwardExpanderAttackPayload
 * {
 *    uint16_t paramId;       // Must be DownwardExpanderParamId::kAttackCoeff (4)
 *    T        *attackCoeff;  // Array of attack coeff values, one per channel (length: _numChannels, Range: [0.0, 1.0])
 * } DownwardExpanderAttackPayload;
 *
 * // 6. Release Coefficient Payload (DownwardExpanderParamId::kReleaseCoeff)
 * typedef struct DownwardExpanderReleasePayload
 * {
 *    uint16_t paramId;       // Must be DownwardExpanderParamId::kReleaseCoeff (5)
 *    T        *releaseCoeff; // Array of release coeff values, one per channel
 *                            // (length: _numChannels, Range: [0.0, 1.0])
 * } DownwardExpanderReleasePayload;
 *
 * // 7. Hold Payload (DownwardExpanderParamId::kHoldSamples)
 * typedef struct DownwardExpanderHoldPayload
 * {
 *    uint16_t paramId;      // Must be DownwardExpanderParamId::kHoldSamples (6)
 *    T        *holdSamples; // Array of hold times in samples, one per channel (length: _numChannels, Range: [0.0, 16777215.0])
 * } DownwardExpanderHoldPayload;
 *
 * // 8. All Payload (DownwardExpanderParamId::kAll): one DownwardExpanderTunableParams<T>; every array is
 * //    copied in (setParams) or out (getParams), all six array pointers must be non-null.
 *
 * Setting kBypassMode back to 0 discards the gain-smoothing and hold state, because it stopped tracking the
 * signal while bypass was engaged.
 * @endcode
 *
 * The coefficients map onto the MATLAB `expander` System object properties as
 * attackCoeff = exp(-log(9) / (SampleRate * AttackTime)),
 * releaseCoeff = exp(-log(9) / (SampleRate * ReleaseTime)) and
 * holdSamples = SampleRate * HoldTime (pass the unrounded product: the hold lasts floor(holdSamples) samples, as in MATLAB).
 */
/**
 * <b>Runtime Control Philosophy:</b>
 * The DownwardExpander node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class DownwardExpanderParamId : uint16_t {
   kBypassMode   = 0, /**< kBypassMode: `0` applies the expander, `1` copies input to output */
   kThresholdDb  = 1, /**< kThresholdDb: Threshold values to set/get for each channel */
   kRatio        = 2, /**< kRatio: Expansion ratio values to set/get for each channel */
   kKneeWidthDb  = 3, /**< kKneeWidthDb: Knee width values to set/get for each channel */
   kAttackCoeff  = 4, /**< kAttackCoeff: Attack coeff values to set/get for each channel */
   kReleaseCoeff = 5, /**< kReleaseCoeff: Release coeff values to set/get for each channel */
   kHoldSamples  = 6, /**< kHoldSamples: Hold time in samples to set/get for each channel */
   kAll          = 7  /**< kAll: all tunable params as one @ref DownwardExpanderTunableParams payload */
};

/**
 * @brief Tunable parameters of the @ref DownwardExpander node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref DownwardExpanderParamId::kAll payload of
 *          @ref DownwardExpander::setParams / @ref DownwardExpander::getParams. Each array is caller-owned,
 *          holds one value per channel, is required, and must outlive the node.
 *
 * @tparam T : Node element type (float).
 */
template <typename T> struct DownwardExpanderTunableParams {
   T       *pThresholdDb  = nullptr; /*!< Threshold in dB per channel, range <= 0. */
   T       *pRatio        = nullptr; /*!< Expansion ratio per channel, range >= 1. */
   T       *pKneeWidthDb  = nullptr; /*!< Knee width in dB per channel, range >= 0. */
   T       *pAttackCoeff  = nullptr; /*!< Attack smoothing coefficient per channel, range [0, 1]. */
   T       *pReleaseCoeff = nullptr; /*!< Release smoothing coefficient per channel, range [0, 1]. */
   T       *pHoldSamples  = nullptr; /*!< Hold length in samples per channel, range [0, 16777215]; 0 disables it. */
   uint32_t bypassMode    = 0U;      /*!< `0`: apply the expander, `1`: copy input to output. */
};

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 *  \class DownwardExpander
 *  @brief Provides node-level APIs for dynamic range expander algorithm using AUDIOLIB kernels.
 *  @tparam T  : Supports float
 *
 *  @details The algorithm attenuates a signal that falls below a specified threshold,
 *  expanding the dynamic range of the signal (MATLAB `expander` System object). Below the
 *  threshold the output level falls by the expansion ratio per dB of input level; the
 *  transition region is specified by the knee policy (hard knee for a knee width of 0).
 *
 *  Static characteristic (T = threshold, R = ratio, W = knee width), in dB:
 *  - x < T - W/2             : x_sc = T + (x - T) * R
 *  - T - W/2 <= x <= T + W/2 : x_sc = x + (1 - R) * (x - T - W/2)^2 / (2W)
 *  - x > T + W/2             : x_sc = x
 *
 *  The computed gain g_c = x_sc - x is post-processed with a one-pole filter with
 *  user-provided attack/release coefficients (g_s = smoothed gain, C = hold counter):
 *  - g_c > g_s[n-1] (release) : C = 0, g_s = releaseCoeff * g_s[n-1] + (1 - releaseCoeff) * g_c
 *  - g_c < g_s[n-1] (attack)  : C = C + 1, g_s = g_s[n-1] while C <= holdSamples,
 *                               g_s = attackCoeff * g_s[n-1] + (1 - attackCoeff) * g_c afterwards
 *  - g_c = g_s[n-1]           : C unchanged, g_s = g_s[n-1]   (measured MATLAB behaviour)
 *
 *  The level x = 20*log10|input| is floored at -313.0712 dB (20*log10(DBL_EPSILON)), as in the MATLAB expander.
 *  There is no make-up gain: a signal above the knee passes through at unity gain.
 *
 *  All buffers are expected to be in interleaved format, i.e. each
 *  column contains an independent channel of sample sequences. The tunable
 *  parameter buffers will be independently applied to each channel. For best
 *  performance, user should provide a channel count at least the size of
 *  SIMD width.
 *
 */

template <typename T> class DownwardExpander final : public VirtualOp {

   NodeInfo       _nodeInfo;     /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;           /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;          /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   T             *_scratch;      /*!< Scratch matrix (numChannels x blockSize, packed rows) */
   T             *_gainState;    /*!< Per-channel gain-smoothing state buffer */
   T             *_holdCounter;  /*!< Per-channel hold counter state buffer (samples, held as T) */
   T             *_thresholdDb;  /*!< Caller-owned threshold array (dB), used live by exec and setParams */
   T             *_ratio;        /*!< Caller-owned expansion ratio array, used live by exec and setParams */
   T             *_kneeWidthDb;  /*!< Caller-owned knee width array (dB), used live by exec and setParams */
   T             *_attackCoeff;  /*!< Caller-owned attack coefficient array, used live by exec and setParams */
   T             *_releaseCoeff; /*!< Caller-owned release coefficient array, used live by exec and setParams */
   T             *_holdSamples;  /*!< Caller-owned hold length array (samples), used live by exec and setParams */
   size_t         _numChannels;  /*!< Number of channels (input width) */
   size_t         _blockSize;    /*!< Number of samples per channel per block (input height) */
   uint32_t       _bypassMode;   /*!< Bypass mode: `0` applies the expander, `1` copies input to output */

   TISP::HandlePtr<char> _handle_dB20;         /*!< handle required by AUDIOLIB_dB20 kernel */
   TISP::HandlePtr<char> _handle_undB20;       /*!< handle required by AUDIOLIB_undB20 kernel */
   TISP::HandlePtr<char> _handle_mul;          /*!< handle for the internal multiply stage */
   TISP::HandlePtr<char> _handle_abs;          /*!< handle for the internal abs stage */
   TISP::HandlePtr<char> _handle_gainComputer; /*!< handle for the internal gain-computer stage */

   // Private functions used in expander algorithm

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
   TISP_downwardExpander_abs_init(void *handle, AUDIOLIB_bufParams2D_t *bufParamsIn, AUDIOLIB_bufParams2D_t *bufParamsOut);

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
   int32_t TISP_downwardExpander_mul_init(void                   *handle,
                                  AUDIOLIB_bufParams2D_t *bufParamsIn0,
                                  AUDIOLIB_bufParams2D_t *bufParamsIn1,
                                  AUDIOLIB_bufParams2D_t *bufParamsOut);

   /**
    * @brief Writes the absolute value of each input sample to the output matrix
    *
    * @param[in]  handle : Handle holding the SE/SA descriptors set by @ref TISP_downwardExpander_abs_init
    * @param[in]  pIn    : Pointer to the input matrix
    * @param[out] pOut   : Pointer to the output matrix
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_abs_exec)
   inline void TISP_downwardExpander_abs_exec(void *handle, T *pIn, T *pOut);

   /**
    * @brief Writes the element-wise product of two input matrices to the output matrix
    *
    * @param[in]  handle : Handle holding the SE/SA descriptors set by @ref TISP_downwardExpander_mul_init
    * @param[in]  pIn0   : Pointer to the first input matrix
    * @param[in]  pIn1   : Pointer to the second input matrix
    * @param[out] pOut   : Pointer to the output matrix
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_mul_exec)
   inline void TISP_downwardExpander_mul_exec(void *handle, T *pIn0, T *pIn1, T *pOut);

   /**
    * @brief Configures the SE/SA descriptors for the internal gain-computer stage
    *
    * @param[in]  handle        : Handle the SE/SA descriptors are written into
    * @param[in]  bufParamsIn   : Buffer parameters of the input matrix
    * @param[in]  bufParamsGain : Buffer parameters of the per-channel parameter and state vectors
    * @param[in]  bufParamsOut  : Buffer parameters of the output matrix
    * @retval     int32_t       : `0` on success
    * @throw None
    */
   int32_t TISP_downwardExpander_gainComputer_init(void                   *handle,
                                           AUDIOLIB_bufParams2D_t *bufParamsIn,
                                           AUDIOLIB_bufParams1D_t *bufParamsGain,
                                           AUDIOLIB_bufParams2D_t *bufParamsOut);

   /**
    * @brief Computes the per-sample expander gain from the dB-domain input
    *
    * Applies the knee policy and the attack/hold/release one-pole smoothing, writing the
    * smoothed gain to the output matrix.
    *
    * @param[in]  handle : Handle holding the SE/SA descriptors set by @ref TISP_downwardExpander_gainComputer_init
    * @param[in]  pIn    : Pointer to the dB-domain input matrix
    * @param[out] pOut   : Pointer to the output matrix
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_gainComputer_exec)
   inline void TISP_downwardExpander_gainComputer_exec(void *handle, T *pIn, T *pOut);

   /**
    * @brief Writes the target gain of the first 2 * @p numPairs channel blocks over their levels
    *
    * @param[in]     handle   : Handle holding the SE/SA descriptors set by @ref TISP_downwardExpander_gainComputer_init
    * @param[in,out] pInOut   : Pointer to the dB-domain input matrix; receives the target gains
    * @param[in]     numPairs : Number of channel-block pairs
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_targetGain_exec)
   inline void TISP_downwardExpander_targetGain_exec(void *handle, T *pInOut, uint32_t numPairs);

   /**
    * @brief Smooths the target gains of the first 2 * @p numPairs channel blocks, two blocks per iteration
    *
    * @param[in]  handle   : Handle holding the SE/SA descriptors set by @ref TISP_downwardExpander_gainComputer_init
    * @param[in]  pIn      : Pointer to the target-gain matrix written by @ref TISP_downwardExpander_targetGain_exec
    * @param[out] pOut     : Pointer to the output matrix
    * @param[in]  numPairs : Number of channel-block pairs
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_smoothPairs_exec)
   inline void TISP_downwardExpander_smoothPairs_exec(void *handle, T *pIn, T *pOut, uint32_t numPairs);

   /**
    * @brief Computes target gain and smoothing in one loop for the channel blocks from @p firstBlock on
    *
    * @param[in]  handle     : Handle holding the SE/SA descriptors set by @ref TISP_downwardExpander_gainComputer_init
    * @param[in]  pIn        : Pointer to the dB-domain input matrix
    * @param[out] pOut       : Pointer to the output matrix
    * @param[in]  firstBlock : First channel block to process
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_gainFused_exec)
   inline void TISP_downwardExpander_gainFused_exec(void *handle, T *pIn, T *pOut, uint32_t firstBlock);

   /**
    * @brief Derives the per-channel knee breakpoints and coefficients used by @ref TISP_downwardExpander_targetGain_vec
    *
    * @tparam     vecType        : C7x vector type of the operands
    * @param[in]  vThreshold     : Threshold T in dB
    * @param[in]  vRatio         : Expansion ratio R
    * @param[in]  vKneeWidth     : Knee width W in dB (hard knee for W <= 0)
    * @param[out] vKneeLower     : Lower knee edge S = T - W/2
    * @param[out] vKneeUpper     : Upper knee edge E = T + W/2
    * @param[out] vSoftKneeCoeff : Quadratic knee coefficient (1 - R) / (2W), zero for a hard knee
    * @param[out] vRatioMinusOne : R - 1
    * @retval None
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_kneeParams_exec)
   template <typename vecType>
   inline void TISP_downwardExpander_kneeParams_exec(vecType  vThreshold,
                                             vecType  vRatio,
                                             vecType  vKneeWidth,
                                             vecType &vKneeLower,
                                             vecType &vKneeUpper,
                                             vecType &vSoftKneeCoeff,
                                             vecType &vRatioMinusOne);

   /**
    * @brief Computes the target gain x_sc - x in dB for a vector of levels in dB
    *
    * @tparam     vecType : C7x vector type of the operands
    * @retval     vecType : Target gain in dB
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_targetGain_vec)
   template <typename vecType>
   inline vecType TISP_downwardExpander_targetGain_vec(vecType vIn,
                                               vecType vKneeLower,
                                               vecType vKneeUpper,
                                               vecType vSoftKneeCoeff,
                                               vecType vRatioMinusOne);

   /**
    * @brief One attack/hold/release smoothing step
    *
    * @tparam        vecType    : C7x vector type of the operands
    * @param[in,out] vHoldCount : Hold counter, updated in place
    * @retval        vecType    : New smoothed gain in dB
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_smoothStep_vec)
   template <typename vecType>
   inline vecType TISP_downwardExpander_smoothStep_vec(vecType  vTargetGain,
                                               vecType  vSmoothedGain,
                                               vecType &vHoldCount,
                                               vecType  vAttackCoeff,
                                               vecType  vOneMinusAttackCoeff,
                                               vecType  vReleaseCoeff,
                                               vecType  vOneMinusReleaseCoeff,
                                               vecType  vHoldSamples);

   /**
    * @brief Computes the vector reciprocal used by the gain computer
    *
    * @tparam     vecType : C7x vector type of the operand
    * @param[in]  vIn     : Input vector
    * @retval     vecType : Element-wise reciprocal of @p vIn
    * @throw None
    */
   __INLINE_FUNC(TISP_downwardExpander_div_exec)
   template <typename vecType> inline vecType TISP_downwardExpander_div_exec(vecType vIn);

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
    * @brief Creates a downward expander node.
    *
    * @details Validates the buffer descriptors and initializes the internal stages. Buffers are interleaved:
    *          columns are channels and rows are samples; both state buffers are cleared.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one interleaved buffer. Its @c pBuf is the
    *                                    initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one interleaved buffer with the input's
    *                                    width and height.
    * @param [in] pStates               : State buffer descriptors; exactly two buffers, each with a width of one value
    *                                    per channel: [0] gain-smoothing state, [1] hold counter.
    * @param [in] pScratch              : Scratch buffer; required.
    * @param [in] scratchBytes          : Size of @c pScratch in bytes; at least channels x samples elements.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with all six arrays non-null.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   DownwardExpander(const TISP_BufDescList                     *pInputs,
                        const TISP_BufDescList                     *pOutputs,
                        const TISP_BufDescList                     *pStates,
                        void                                       *pScratch,
                        size_t                                      scratchBytes,
                        const DownwardExpanderTunableParams<T> *pDefaultTunableParams,
                        const char                                 *nodeName,
                        uint32_t                                    nodeId,
                        ErrorCtxt                                  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _in (input data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _out (output data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _in)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _out)
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
    * @details Zeroes the per-channel gain-smoothing state in @ref _gainState and the hold
    *          counters in @ref _holdCounter, which are the node's only internal state. Called by
    *          the constructor once initialization has succeeded, so the envelope always starts
    *          from rest, and by @ref setParams when @ref DownwardExpanderParamId::kBypassMode returns from
    *          `1` to `0`, since the envelope froze while bypassed. Call it directly to restart a
    *          stream without reconstructing the node.
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

   ~DownwardExpander() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
