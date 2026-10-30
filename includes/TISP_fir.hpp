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
 *  @details Applies a finite impulse response filter to each channel (row) of a non-interleaved input block.
 *           The state buffer carries the last `filterSize - 1` input samples of every channel across blocks.
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The Fir node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters (such as the active filter coefficients) without needing
 * to reconstruct the node.
 *
 */
enum class FirParamId : uint16_t {
   kBypass       = 0, /**< kBypass: uint32_t bypass mode; `0` applies the filter, `1` copies input to output */
   kFilterCoeffs = 1, /**< kFilterCoeffs: Filter coefficients to set/get, `filterSize` values per coefficient row */
   kAll          = 2  /**< kAll: all tunable params as one @ref FirTunableParams payload */
};

/**
 * @brief Construction-time (non-tunable) parameters of the @ref Fir node.
 *
 * @details Required; describes the filter and the layout of the coefficient buffer
 *          (@ref FirTunableParams::pCoeffs).
 */
struct FirConfig {
   size_t filterSize      = 0U;    /*!< Number of filter taps. */
   bool   enableNchCoefs  = false; /*!< `true`: one coefficient row per channel, `false`: one row shared by all. */
   size_t coeffPitchBytes = 0U;    /*!< Row pitch of the coefficient buffer in bytes. */
   bool   enableMMA       = false; /*!< `true`: MMA-based computation; coefficient rows use the MMA-padded layout. */
   bool   enableQ         = false; /*!< `true`: fixed Q format for MMA-based computation (fixed-point MMA SoCs). */
   size_t Q               = 0U;    /*!< Q value for Q-enabled computation. */
};

/**
 * @brief Tunable parameters of the @ref Fir node, with their default values.
 *
 * @details Initial values of the runtime parameters, and the @ref FirParamId::kAll payload of
 *          @ref Fir::setParams / @ref Fir::getParams.
 *
 * @tparam T : Node element type (float).
 */
template <typename T> struct FirTunableParams {
   T       *pCoeffs    = nullptr; /*!< Caller-owned coefficient buffer in kernel layout (one row, or one per channel,
                                       at @ref FirConfig::coeffPitchBytes); required, must outlive the node. */
   uint32_t bypassMode = 0U;      /*!< `0`: apply the filter, `1`: copy input to output. */
};

template <typename T> class Fir final : public VirtualOp {

   NodeInfo       _nodeInfo;        /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;              /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;             /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _state;           /*!< State buffer descriptor (filter history followed by the current block) */
   size_t         _filterSize;      /*!< Number of filter taps */
   size_t         _numCoeffRows;    /*!< Number of coefficient rows (number of channels, or 1 when shared) */
   size_t         _coeffPitchBytes; /*!< Row pitch of the coefficient buffer in bytes */
   size_t         _coeffOffset;     /*!< Offset of the first tap within a coefficient row, in elements */
   T             *_coeffs;          /*!< Caller-owned coefficient buffer, used live by exec and setParams */
   uint32_t       _bypassMode;      /*!< Bypass mode: `0` applies the filter, `1` copies input to output */

   // input to state
   __SE_TEMPLATE_v1 _se0ParamsPre = __gen_SE_TEMPLATE_v1(); /*!< SE params for the input-to-state block copy */
   __SA_TEMPLATE_v1 _sa0ParamsPre = __gen_SA_TEMPLATE_v1(); /*!< SA params for the input-to-state block copy */

   // state delay after FIR block processing
   __SE_TEMPLATE_v1 _se0ParamsPost = __gen_SE_TEMPLATE_v1(); /*!< SE params for the state delay-line block copy */
   __SA_TEMPLATE_v1 _sa0ParamsPost = __gen_SA_TEMPLATE_v1(); /*!< SA params for the state delay-line block copy */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level DSPLIB_fir kernel */

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
    * @brief Copies the input buffer described by @ref _in to the output buffer described by @ref _out,
    *        row by row, honouring both row pitches.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

   /**
    * @brief Copies packed coefficient rows (`filterSize` values each) into the coefficient buffer.
    *
    * @param [in] pPacked : Packed coefficients, @ref _numCoeffRows rows of @ref _filterSize values.
    *
    * @retval None
    * @throw None
    */
   void writePackedCoeffs(const T *pPacked) noexcept;

   /**
    * @brief Copies the coefficient buffer into packed rows (`filterSize` values each).
    *
    * @param [out] pPacked : Destination for @ref _numCoeffRows rows of @ref _filterSize values.
    *
    * @retval None
    * @throw None
    */
   void readPackedCoeffs(T *pPacked) const noexcept;

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
    * @brief Creates a DSPLIB_fir node.
    *
    * @details Validates the buffer descriptors and initializes the DSPLIB_fir kernel. Rows are channels and
    *          columns are samples; the state buffer is cleared.
    *
    * @param [in] pInputs               : Input buffer descriptors; exactly one non-interleaved buffer. Its @c pBuf is
    *                                    the initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs              : Output buffer descriptors; exactly one non-interleaved buffer with the
    *                                    input's width and height.
    * @param [in] pStates               : State buffer descriptors; exactly one non-interleaved buffer with the input's
    *                                    height and a width of input width + `filterSize` - 1.
    * @param [in] pConfig          : Construction-time parameters; required.
    * @param [in] pDefaultTunableParams : Initial tunable parameters; required, with a non-null @c pCoeffs.
    * @param [in] nodeName              : Name of the node
    * @param [in] nodeId                : ID of the node
    * @param [in] errorCtx              : Error context for reporting constructor errors
    *
    * @throw None
    */
   Fir(const TISP_BufDescList    *pInputs,
       const TISP_BufDescList    *pOutputs,
       const TISP_BufDescList    *pStates,
       const FirConfig      *pConfig,
       const FirTunableParams<T> *pDefaultTunableParams,
       const char                *nodeName,
       uint32_t                   nodeId,
       ErrorCtxt                 &errorCtx);

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
    * @brief Clean and reset all internal state buffers to zero
    *
    * @retval None
    * @throw None
    */
   void cleanInternalBuffers() noexcept override;

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
    * @brief Destructor
    *
    * This destructor is set to default.
    */
   ~Fir() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
