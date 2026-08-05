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
 * \class Nlms
 * @brief Provides node-level APIs for the AUDIOLIB_nlms kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel implements the Normalized Least Mean Squares (NLMS) Adaptive Filter, which is used for adaptive
 * filtering to estimate a clean signal from a noisy input. It adjusts the noisy input signal pIn to match the desired
 * signal pInDesired by minimizing the error between the clean reference and the filtered output pOut.
 *
 * It supports both interleaved and non-interleaved formats of data.
 *
 */

template <typename T> class Nlms final : public VirtualOp {

   T *_pIn;            /*!< Pointer to input matrix. */
   T *_pInDesired;     /*!< Pointer to desired signal buffer. */
   T *_pStateBuffer;   /*!< Pointer to circular state buffer. */
   T *_pScratchBuffer; /*!< Pointer to accumulator buffer. */
   T *_pCoefficients;  /*!< Pointer to coefficients buffer. */
   T *_pOut;           /*!< Pointer to output matrix. */
   T *_pInD;           /*!< Pointer to deinterleave input matrix. */
   T *_pInDesiredD;    /*!< Pointer to deinterleave desired signal buffer. */
   T *_pOutI;          /*!< Pointer to interleave output matrix. */

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   size_t _widthIn;
   size_t _heightIn;
   size_t _filterLength;
   float  _stepSize;
   size_t _numChannels;
   size_t _inPitchBytes;
   size_t _outPitchBytes;
   size_t _isInterleave;
   size_t _historyBufSize; /*!< Size of history buffer. */

   TISP::HandlePtr<char> _handle;  /*!< Handle to low level AUDIOLIB_nlms kernel. */
   TISP::HandlePtr<char> _handle1; /*!< Handle to low level DSPLIB_deinterleave kernel. */
   TISP::HandlePtr<char> _handle2; /*!< Handle to low level DSPLIB_interleave kernel. */

   void (Nlms::*_execImpl)() noexcept; /*!< Function pointer bound at construction time for zero-overhead exec. */

   void _execNonInterleaved() noexcept;
   void _execInterleaved() noexcept;

 public:
   /**
    * @brief Creates an AUDIOLIB_nlms node.
    *
    * This constructor initializes a node for the AUDIOLIB_nlms kernel.
    *
    * @param [in]  *pIn            : Pointer to the input matrix.
    * @param [in]  *pInDesired     : Pointer to the desired signal buffer.
    * @param [in]  *pStateBuffer   : Pointer to circular state buffer.
    * @param [in]  *pScratchBuffer : Pointer to accumulator buffer.
    * @param [in]  *pCoefficients  : Pointer to coefficients buffer.
    * @param [out] *pOut           : Pointer to the output matrix.
    * @param [in]  *pInD           : Pointer to the deinterleave input matrix.
    * @param [in]  *pInDesiredD    : Pointer to the deinterleave desired signal buffer.
    * @param [out] *pOutD          : Pointer to the interleave output matrix.
    * @param [in]  widthIn         : Width of input matrix.
    * @param [in]  heightIn        : Height of input matrix.
    * @param [in]  filterLength    : Length of the adaptive filter.
    * @param [in]  stepSize        : Adaptive step size (mu).
    * @param [in]  numChannels     : Number of audio channels.
    * @param [in]  inPitchBytes    : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes   : Pitch of output matrix in bytes.
    * @param [in]  isInterleave    : Indicates whether the input matrix data is interleaved:
    *                              - `0`: Non-interleaved (Rows represent channels, columns represent samples).
    *                              - `1`: Interleaved (Rows represent samples, columns represent channels).
    * @param [in]  historyBufSize  : Size of history buffer. Minimum size is 512 bytes, rounded up to the nearest power
    * of 2 to accommodate filterLength + widthIn.
    * @param [in]  errorCtx        : Error context for reporting constructor errors
    * @retval None
    * @throw None
    *
    */

   Nlms(T         *pIn,
        T         *pInDesired,
        T         *pStateBuffer,
        T         *pScratchBuffer,
        T         *pCoefficients,
        T         *pOut,
        T         *pInD,
        T         *pInDesiredD,
        T         *pOutD,
        size_t     widthIn,
        size_t     heightIn,
        size_t     filterLength,
        float      stepSize,
        size_t     numChannels,
        size_t     inPitchBytes,
        size_t     outPitchBytes,
        size_t     isInterleave,
        size_t     historyBufSize,
        ErrorCtxt &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_nlms node with a name and ID
    *
    * This constructor is a delegator that accepts a node name and ID.
    *
    * @param [in]  *pIn            : Pointer to the input matrix.
    * @param [in]  *pInDesired     : Pointer to the desired signal buffer.
    * @param [in]  *pStateBuffer   : Pointer to circular state buffer.
    * @param [in]  *pScratchBuffer : Pointer to accumulator buffer.
    * @param [in]  *pCoefficients  : Pointer to coefficients buffer.
    * @param [out] *pOut           : Pointer to the output matrix.
    * @param [in]  *pInD           : Pointer to the deinterleave input matrix.
    * @param [in]  *pInDesiredD    : Pointer to the deinterleave desired signal buffer.
    * @param [out] *pOutD          : Pointer to the interleave output matrix.
    * @param [in]  widthIn         : Width of input matrix.
    * @param [in]  heightIn        : Height of input matrix.
    * @param [in]  filterLength    : Length of the adaptive filter.
    * @param [in]  stepSize        : Adaptive step size (mu).
    * @param [in]  numChannels     : Number of audio channels.
    * @param [in]  inPitchBytes    : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes   : Pitch of output matrix in bytes.
    * @param [in]  isInterleave    : Indicates whether the input matrix data is interleaved.
    * @param [in]  historyBufSize  : Size of history buffer. Minimum size is 512 bytes, rounded up to the nearest power
    * of 2 to accommodate filterLength + widthIn.
    * @param [in]  nodeName        : Name of the node; initializes @ref _nodeInfo.
    * @param [in]  nodeId          : ID of the node; initializes @ref _nodeInfo.
    * @param [in]  errorCtx        : Error context for reporting constructor errors
    * @retval None
    * @throw None
    */

   Nlms(T          *pIn,
        T          *pInDesired,
        T          *pStateBuffer,
        T          *pScratchBuffer,
        T          *pCoefficients,
        T          *pOut,
        T          *pInD,
        T          *pInDesiredD,
        T          *pOutD,
        size_t      widthIn,
        size_t      heightIn,
        size_t      filterLength,
        float       stepSize,
        size_t      numChannels,
        size_t      inPitchBytes,
        size_t      outPitchBytes,
        size_t      isInterleave,
        size_t      historyBufSize,
        const char *nodeName,
        uint32_t    nodeId,
        ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pIn (input data matrix) - managed by setAddr()
    * - index 1: _pInDesired (desired signal matrix)
    *
    * @param[in]  index : Index of the input buffer (0 or 1)
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
    * @retval    size_t : The number of input buffers (2: _pIn and _pInDesired)
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
    * @param [in] addrs  : Structure containing source and destination addresses
    * @param [in] mode   : Mode of address setting (input, output, or both)
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

   ~Nlms() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
