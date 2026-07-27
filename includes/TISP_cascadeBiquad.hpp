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
 *  @details This node applies a cascade of second-order (biquad) sections to each channel of the
 *  input buffer pIn and writes the filtered values to the output buffer pOut. The sections are
 *  applied in series, the output of one stage feeding the next. Input and output are interleaved:
 *  each row holds one sample across all channels. The per-channel filter state is carried in
 *  pFilterVar across calls, so blocks must be presented in order. The coefficient buffer
 *  pFilterCoeff is shared by all channels unless `enableNChCoeffs` is set, and its layout depends
 *  on the selected `biquadForm` (DF1, DF2, or TDF2).
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The CascadeBiquad node supports dynamic parameter updates via a robust runtime API. Users can query
 * or modify specific algorithmic parameters (such as filter coefficients) without needing to
 * reconstruct the node.
 *
 */
enum class CascadeBiquadParamId : uint16_t {
   kFilterCoeffs = 0, /**< kFilterCoeffs: Filter coefficients block */
   kBypass       = 1  /**< kBypass: uint32_t bypass mode; `0` applies the filter, `1` copies input to output */
};

template <typename T> class CascadeBiquad final : public VirtualOp {

   T       *_pIn;          /*!< Pointer to input buffer */
   T       *_pFilterCoeff; /*!< Pointer to filter coefficients */
   T       *_pFilterVar;   /*!< Pointer to state buffer */
   T       *_pOut;         /*!< Pointer to output buffer */
   NodeInfo _nodeInfo;     /*!< Node name, parameters, and ID */

   size_t _size;              /*!< Number of samples per channel. */
   size_t _numChannels;       /*!< Number of channels. */
   size_t _dataPitchInBytes;  /*!< Pitch of the input buffer in bytes. */
   size_t _dataPitchOutBytes; /*!< Pitch of the output buffer in bytes. */

   uint32_t _bypassMode; /*!< Bypass mode: `0` applies the filter, `1` copies input to output. */

   TISP::HandlePtr<char> _handle;          /*!< handle required by DSPLIB_cascadeBiquad kernel */
   size_t                _coeffSizeBytes;  /*!< Cached size in bytes of the filter coefficients buffer */
   size_t                _stateBufferSize; /*!< Cached size in bytes of the state buffer @ref _pFilterVar */

   /**
    * @brief Copies @ref _size rows of @ref _numChannels elements from @ref _pIn to @ref _pOut,
    *        honouring @ref _dataPitchInBytes and @ref _dataPitchOutBytes
    *
    * Used by the bypass path. The state buffer @ref _pFilterVar is not advanced while bypass is
    * enabled, so @ref cleanInternalBuffers() is invoked when bypass is disengaged to discard the
    * now-stale filter history.
    *
    * @retval None
    * @throw None
    */
   void bypassCopy() noexcept;

 public:
   /**
    * @brief Creates a DSPLIB_CascadeBiquad node
    *
    * This constructor is called to create the node
    *
    * @param[in]  *pIn              : Pointer to input buffer; initializes _pIn
    * @param[in]  *pFilterCoeff     : Pointer to filter coefficient buffer; initializes _pFilterCoeff
    * @param[in]  *pFilterVar       : Pointer for node to hold state buffer; initializes _pFilterVar
    * @param[out] *pOut             : Pointer to output buffer; initializes _pOut
    * @param[in]  size              : Number of samples per channel; initializes _size
    * @param[in]  numChannels       : Number of channels; initializes _numChannels
    * @param[in]  numStages         : Number of biquad stages in the cascade; initializes _numStages
    * @param[in]  biquadForm        : One of the following; initializes _biquadForm
    *                                 - DSPLIB_BIQUAD_DF1
    *                                 - DSPLIB_BIQUAD_DF2
    *                                 - DSPLIB_BIQUAD_TDF2
    * @param[in]  dataPitchInBytes  : Pitch of input buffer in bytes; initializes _dataPitchInBytes
    * @param[in]  dataPitchOutBytes : Pitch of output buffer in bytes; initializes _dataPitchOutBytes
    * @param[in]  filterVarPitchBytes : Pitch of the state buffer in bytes; initializes _filterVarPitchBytes
    * @param[in]  enableNChCoeffs   : Every channel has its own set of filter coefficients (T/F);
    * @param [in]  errorCtx          : Error context for reporting constructor errors
    *
    */

   CascadeBiquad(T         *pIn,
                 T         *pFilterCoeff,
                 T         *pFilterVar,
                 T         *pOut,
                 size_t     size,
                 size_t     numChannels,
                 size_t     numStages,
                 uint32_t   biquadForm,
                 size_t     dataPitchInBytes,
                 size_t     dataPitchOutBytes,
                 size_t     filterVarPitchBytes,
                 bool       enableNChCoeffs,
                 ErrorCtxt &errorCtx);

   /**
    * @brief Creates a DSPLIB_CascadeBiquad node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */

   CascadeBiquad(T          *pIn,
                 T          *pFilterCoeff,
                 T          *pFilterVar,
                 T          *pOut,
                 size_t      size,
                 size_t      numChannels,
                 size_t      numStages,
                 uint32_t    biquadForm,
                 size_t      dataPitchInBytes,
                 size_t      dataPitchOutBytes,
                 size_t      filterVarPitchBytes,
                 bool        enableNChCoeffs,
                 const char *nodeName,
                 uint32_t    nodeId,
                 ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pIn (input data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pOut (output data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _pIn)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pOut)
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
    * This destructor is set to default
    *
    */

   ~CascadeBiquad() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
