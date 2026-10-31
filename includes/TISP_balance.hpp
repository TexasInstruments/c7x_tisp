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
 * \class Balance
 * @brief Provides node-level APIs for the AUDIOLIB_balance kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel adjusts the stereo balance in an audio signal of the input buffer pX (interleaved or
 * non-interleaved) by applying a corresponding gain factor and writes the resulting values to the output buffer pY.
 * <br> The gain factors for left and right channels are calculated based on given balance value.
 *
 */

template <typename T> class Balance final : public VirtualOp {

   T       *_pXL;      /*!< Pointer to input Left matrix. */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pXR; /*!< Pointer to input Right matrix. */
   T *_pYL; /*!< Pointer to output Left matrix. */
   T *_pYR; /*!< Pointer to output Right matrix. */

   size_t  _widthIn;
   size_t  _heightIn;
   size_t  _inPitchBytes;
   size_t  _outPitchBytes;
   float   _balance;
   float   _smoothingTime;
   int32_t _samplingRate;

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_balance kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_balance node.
    *
    * This constructor initializes a node for the AUDIOLIB_balance kernel.
    *
    * @param [in]  pXL           : Pointer to the input left matrix.
    *			                         Initializes @ref _pXL.
    * @param [in]  pXR           : Pointer to the input right matrix.
    *			                         Initializes @ref _pXR.
    * @param [out] pYL           : Pointer to the output left matrix.
    *			                         Initializes @ref _pYL.
    * @param [out] pYR           : Pointer to the output right matrix.
    *			                         Initializes @ref _pYR.
    * @param [in]  widthIn       : Width of input matrix.
    * @param [in]  heightIn      : Height of input matrix.
    * @param [in]  inPitchBytes  : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes : Pitch of output matrix in bytes.
    * @param [in]  balance       : Panning control parameter. Range: -1 (full left) to 1 (full right) with 0
    * representing center balance.
    * @param [in]  smoothingTime : Time constant (in milliseconds) for exponential smoothing of gain transitions. Range:
    * 0 to 1000 msec.
    * @param [in]  samplingRate  : Audio sampling rate (in Hz) defining the number of samples per second.
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    *
    */
   Balance(T         *pXL,
           T         *pXR,
           T         *pYL,
           T         *pYR,
           size_t     widthIn,
           size_t     heightIn,
           size_t     inPitchBytes,
           size_t     outPitchBytes,
           float      balance,
           float      smoothingTime,
           int32_t    samplingRate,
           ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_balance node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Balance(T          *pXL,
           T          *pXR,
           T          *pYL,
           T          *pYR,
           size_t      widthIn,
           size_t      heightIn,
           size_t      inPitchBytes,
           size_t      outPitchBytes,
           float       balance,
           float       smoothingTime,
           int32_t     samplingRate,
           const char *nodeName,
           uint32_t    nodeId,
           ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pXL (input left data matrix) - managed by setAddr()
    * - index 1: _pXR (input right data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0 or 1)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pYL (output left data matrix) - managed by setAddr()
    * - index 1: _pYR (output right data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0 or 1)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers (2: _pXL and _pXR)
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers (2: _pYL and _pYR)
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
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~Balance() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
