// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "../src/common/TISP_common.hpp"
#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <fftlib.h>
#include <memory>

#define TISP_FFT1DBATCHED_I32F_C32FC_O32FC_PBLOCK_SIZE  FFTLIB_FFT1DBATCHED_I32F_C32FC_O32FC_PBLOCK_SIZE

namespace TISP {
namespace FFTLIB {

/**
 *  @ingroup fftlibNode
 */

/**@{*/

/**
 *  \class RFFT1dBatched
 *  @brief Provides node-level APIs for the FFTLIB_rfft1dBatched kernel from FFTLIB.
 *
 *  @tparam T        : Supports float
 */

template <typename T> class RFFT1dBatched final : public VirtualOp {
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pX;    /*!< Pointer to input vector (2D matrix flattened to numChannels * numPoints) */
   T *_pXFFT; /*!< Pointer to temp buffer (2D matrix flattened to numChannels * numPoints) */

   std::shared_ptr<T> _pW;  /*!< Shared pointer to FFT coeff (via TwiddleLUT) */
   std::shared_ptr<T> _pSf; /*!< Shared pointer to split factor coeff (via TwiddleLUT) */

   T *_pY; /*!< Pointer to FFT output */

   FFTLIB_bufParams1D_t _bufParamsX; /*!< FFTLIB bufParams structure of input data */

   uint32_t _numPoints;   /*!< FFT size */
   uint32_t _numChannels; /*!< number of channels */
   bool     _outMode;     /*!< 0 == half spectrum, 1 = full spectrum */

   TISP::HandlePtr<uint8_t> _pBlock; /*!< Pointer with address location of SE and SA params */

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
    * @brief Creates a FFTLIB_rfft1dBatched node
    * @param [in]  pX          : Pointer to input; initializes @ref _pX
    * @param [in]  *pXFFT      : Pointer to FFT scratch buffer
    * @param [out] pY          : Pointer to FFT output; initializes @ref _pY
    * @param [in]  numPoints   : FFT size
    * @param [in]  numChannels : Number of FFT channels
    * @param [in]  outMode     : Output mode flag
    * @param [in]  errorCtx    : Error context for reporting constructor errors
    */

   RFFT1dBatched(T *pX, T *pXFFT, T *pY, uint32_t numPoints, uint32_t numChannels, bool outMode, ErrorCtxt &errorCtx);

   /**
    * @brief Creates a RFFT1dBatched node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   RFFT1dBatched(T          *pX,
                 T          *pXFFT,
                 T          *pY,
                 uint32_t    numPoints,
                 uint32_t    numChannels,
                 bool        outMode,
                 const char *nodeName,
                 uint32_t    nodeId,
                 ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pX (input array)
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pY (output array)
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
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~RFFT1dBatched() = default;
};

/** @} */

} // namespace FFTLIB
} // namespace TISP
