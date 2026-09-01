// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_common.hpp"
#include "TISP_ErrorCtxt.hpp"
#include <memory>

namespace TISP {

namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 * \class Concat
 * @brief Provides node-level APIs for the AUDIOLIB_concat kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel concatenates multiple input arrays into a single output array.
 *          Each input may carry a different number of channels; the output holds the
 *          sum of all input channels. The output uses the same layout as the input
 *          (interleaved or planar).
 */

template <typename T> class Concat final : public VirtualOp {

   std::unique_ptr<void*[]> _inputPtrs; /*!< void* array passed to AUDIOLIB_concat_exec. */
   T                        *_pY;        /*!< Single output pointer. */
   NodeInfo                  _nodeInfo;  /*!< Node name, parameters, and ID. */

   uint32_t _numInputs;       /*!< Number of input buffers to concatenate. */
   uint32_t _totalInChannels; /*!< Total channels across all inputs (sum of inChannels). */
   uint32_t _inSamples;       /*!< Samples per channel. */
   uint8_t  _isInterleave;    /*!< 0=planar, 1=interleaved. */

   TISP::HandlePtr<char> _handle; /*!< Opaque AUDIOLIB kernel handle. */

 public:
   /**
    * @brief Creates a AUDIOLIB_concat node.
    *
    * This constructor initializes a node for the AUDIOLIB_concat kernel.
    *
    * @param [in]   pInputs       : Array of per-input descriptors (one entry per input).
    *                               Each entry carries the input buffer pointer, its
    *                               width/height (pre-oriented per isInterleave), and its
    *                               row stride in bytes.
    * @param [in]   numInputs     : Number of inputs.
    * @param [out]  pY            : Pointer to the output array.
    * @param [in]   outPitchBytes : Pitch in bytes for output array.
    * @param [in]   inSamples     : Samples per channel.
    * @param [in]   isInterleave  : 0=planar, 1=interleaved.
    * @param [in]   errorCtx      : Error context for reporting constructor errors
    */
   Concat(const TISP_BufDesc2D *pInputs,
          uint32_t              numInputs,
          T                    *pY,
          size_t                outPitchBytes,
          uint32_t              inSamples,
          uint8_t               isInterleave,
          ErrorCtxt            &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_concat node with a name and ID.
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Concat(const TISP_BufDesc2D *pInputs,
          uint32_t              numInputs,
          T                    *pY,
          size_t                outPitchBytes,
          uint32_t              inSamples,
          uint8_t               isInterleave,
          const char           *nodeName,
          uint32_t              nodeId,
          ErrorCtxt            &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0 to _numInputs-1: _inputPtrs[index] (input array)
    *
    * @param[in]  index : Index of the input buffer (0 to _numInputs-1)
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
    * @retval    size_t : The number of input buffers (_numInputs: _inputPtrs)
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
    * @param [in] addrs  : Structure containing source and destination addresses.
    * @param [in] mode   : Mode of address setting (input, output, or both).
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
    * @retval const char* The name of the node.
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node
    *
    * This function returns the ID of the node.
    *
    * @retval uint32_t The ID of the node.
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node
    *
    * This function returns a string containing the parameters of the node.
    *
    * @retval const char* The parameters of the node.
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
    * This destructor is set to default.
    *
    */
   ~Concat() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP

/* ======================================================================== */
/*  End of file:  TISP_concat.hpp                                           */
/* ======================================================================== */
