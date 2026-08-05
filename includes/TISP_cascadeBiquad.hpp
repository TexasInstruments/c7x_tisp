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
 */

/**
 * <b>Runtime Control Philosophy:</b>
 * The CascadeBiquad node supports dynamic parameter updates via a robust runtime API. Users can query
 * or modify specific algorithmic parameters (such as filter coefficients) without needing to
 * reconstruct the node.
 *
 */
enum class CascadeBiquadParamId : uint16_t {
   kFilterCoeffs = 0 /**< kFilterCoeffs: Filter coefficients block */
};

template <typename T> class CascadeBiquad final : public VirtualOp {

   T       *_pIn;          /*!< Pointer to input buffer */
   T       *_pFilterCoeff; /*!< Pointer to filter coefficients */
   T       *_pFilterVar;   /*!< Pointer to state buffer */
   T       *_pOut;         /*!< Pointer to output buffer */
   NodeInfo _nodeInfo;     /*!< Node name, parameters, and ID */

   size_t _historyBufSize; /*!< Size of history buffer. */

   TISP::HandlePtr<char> _handle; /*!< handle required by DSPLIB_cascadeBiquad kernel */
   size_t _coeffSizeBytes; /*!< Cached size in bytes of the filter coefficients buffer */

 public:
   /**
    * @brief Creates a DSPLIB_CascadeBiquad node
    *
    * This constructor is called to create the node
    *
    * @param[in]  *pIn              : Pointer to input buffer; initializes @ref _pIn
    * @param[in]  *pFilterCoeff     : Pointer to filter coefficient buffer; initializes @ref _pFilterCoeff
    * @param[in]  *pFilterVar       : Pointer for node to hold state buffer; initializes @ref _pFilterVar
    * @param[out] *pOut             : Pointer to output buffer; initializes @ref _pOut
    * @param[in]  size              : size of input; initializes @ref _size
    * @param[in]  numChannels       : Number of channels; initializes @ref _numChannels
    * @param[in]  numStages         : Number of stages; initializes @ref _numStages
    * @param[in]  biquadForm        : One of the following; initializes @ref _biquadForm
    *                                 - DSPLIB_BIQUAD_DF1
    *                                 - DSPLIB_BIQUAD_DF2
    *                                 - DSPLIB_BIQUAD_TDF2
    * @param[in]  dataPitchInBytes  : Pitch of input buffer in bytes; initializes @ref _dataPitchInBytes
    * @param[in]  dataPitchOutBytes : Pitch of output buffer in bytes; initializes @ref _dataPitchOutBytes
    * @param[in]  filterVarPitchBytes : Pitch of filter buffer in bytes; initializes @ref _filterVarPitchBytes
    * @param[in]  enableNchcoeffs   : Every channel has its own set of filter coeff(T/F);
    * @param [in]  errorCtx          : Error context for reporting constructor errors
    * @retval None
    * @throw None
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
    * This constructor is a delegator that accepts a node name and ID
    *
    * @param[in]  *pIn              : Pointer to input buffer; initializes @ref _pIn
    * @param[in]  *pFilterCoeff     : Pointer to filter coefficient buffer; initializes @ref _pFilterCoeff
    * @param[in]  *pFilterVar       : Pointer for node to hold state buffer; initializes @ref _pFilterVar
    * @param[out] *pOut             : Pointer to output buffer; initializes @ref _pOut
    * @param[in]  size              : size of input; initializes @ref _size
    * @param[in]  numChannels       : Number of channels; initializes @ref _numChannels
    * @param[in]  numStages         : Number of stages; initializes @ref _numStages
    * @param[in]  biquadForm        : One of the following; initializes @ref _biquadForm
    *                                 - DSPLIB_BIQUAD_DF1
    *                                 - DSPLIB_BIQUAD_DF2
    *                                 - DSPLIB_BIQUAD_TDF2
    * @param[in]  dataPitchInBytes  : Pitch of input buffer in bytes; initializes @ref _dataPitchInBytes
    * @param[in]  dataPitchOutBytes : Pitch of output buffer in bytes; initializes @ref _dataPitchOutBytes
    * @param[in]  filterVarPitchBytes : Pitch of filter buffer in bytes; initializes @ref _filterVarPitchBytes
    * @param[in]  enableNchcoeffs   : Every channel has its own set of filter coeff(T/F); initializes
    *                                 @ref _enableNChCoeffs
    * @param[in]  nodeName          : Name of the node; initializes @ref _nodeInfo
    * @param[in]  nodeId            : ID of the node; initializes @ref _nodeInfo
    * @param [in]  errorCtx          : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
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
    * @throw None
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
    * @throw None
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (1: _pIn)
    * @throw None
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (1: _pOut)
    * @throw None
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param[in] addrs Structure containing source and destination addresses
    * @param[in] mode Mode of address setting (input, output, or both)
    * @retval None
    * @throw None
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

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
    * @retval None
    * @throw None
    */

   ~CascadeBiquad() = default;
};

/** @} */

} // namespace DSPLIB

} // namespace TISP
