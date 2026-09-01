// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "../src/common/TISP_common.hpp"
#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <fftlib.h>
#include <memory>

#define TISP_FFT1DBATCHED_I32FC_C32FC_O32FC_PBLOCK_SIZE (13 * SE_PARAM_SIZE)
/* #define TW_HEAP_MEM_SIZE (16 * 1024u) */

namespace TISP {
namespace FFTLIB {

/**
 *  @ingroup fftlibNode
 */

/**@{*/

/**
 *  \class FFT1dBatched
 *  @brief Provides node-level APIs for the FFTLIB_FFT1dBatched kernel from FFTLIB. Assumes complex float data that is
 *         stored in interleaved fashion.
 *  @tparam T        : Supports float
 */

template <typename T> class FFT1dBatched final : public VirtualOp {
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   T *_pX; /*!< Pointer to input vector (2D matrix flattened to numChannels * numPoints) */

   std::shared_ptr<T> _pW; /*!< Shared pointer to FFT coeff (via TwiddleLUT) */

   T *_pY; /*!< Pointer to FFT output */

   FFTLIB_bufParams1D_t _bufParamsX; /*!< FFTLIB bufParams structure of input data */
   FFTLIB_bufParams1D_t _bufParamsW; /*!< FFTLIB bufParams structure of twiddles */
   FFTLIB_bufParams1D_t _bufParamsY; /*!< FFTLIB bufParams structure of output data */

   uint32_t _numPoints;   /*!< FFT size */
   uint32_t _numChannels; /*!< number of channels */

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
    * @brief Creates a FFTLIB_FFT1dBatched node
    * @param [in]  *pX        : Pointer to input; initializes @ref _pX
    * @param [out] *pY        : Pointer to FFT output; initializes @ref _pY
    * @param [in]  numPoints  : FFT size
    * @param [in]  numChannels: Number of FFT channels
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    */

   FFT1dBatched(T *pX, T *pY, uint32_t numPoints, uint32_t numChannels, ErrorCtxt &errorCtx);

   /**
    * @brief Creates a FFT1dBatched node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   FFT1dBatched(T          *pX,
                T          *pY,
                uint32_t    numPoints,
                uint32_t    numChannels,
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

   ~FFT1dBatched() = default;
};

/** @} */

} // namespace FFTLIB
} // namespace TISP
