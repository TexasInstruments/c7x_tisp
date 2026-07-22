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
 * \class Split
 * @brief Provides node-level APIs for the AUDIOLIB_split kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel splits an input array into multiple output arrays. Each output
 *          may carry a different number of channels; the input holds the sum of all
 *          output channels. Each output uses the same layout as the input (interleaved
 *          or planar).
 */

template <typename T> class Split final : public VirtualOp {

   std::unique_ptr<void*[]> _outputPtrs; /*!< void* array passed to AUDIOLIB_split_exec. */
   T                        *_pIn;        /*!< Single input pointer. */
   NodeInfo                  _nodeInfo;   /*!< Node name, parameters, and ID. */

   uint32_t _numOutputs;         /*!< Number of output buffers to split into. */
   uint32_t _totalInputChannels; /*!< Total input channels (sum of outChannels). */
   uint32_t _inSamples;          /*!< Samples per channel. */
   uint8_t  _isInterleave;       /*!< 0=planar, 1=interleaved. */

   TISP::HandlePtr<char> _handle; /*!< Opaque AUDIOLIB kernel handle. */

 public:
   /**
    * @brief Creates a AUDIOLIB_split node.
    *
    * This constructor initializes a node for the AUDIOLIB_split kernel.
    *
    * @param [in]   pIn           : Pointer to the input array.
    * @param [in]   inPitchBytes  : Pitch in bytes for the input array.
    * @param [out]  pOutputs      : Array of per-output descriptors (one entry per output).
    *                               Each entry carries the output buffer pointer, its
    *                               width/height (pre-oriented per isInterleave), and its
    *                               row stride in bytes.
    * @param [in]   numOutputs    : Number of outputs.
    * @param [in]   inSamples     : Samples per channel.
    * @param [in]   isInterleave  : 0=planar, 1=interleaved.
    * @param [in]   errorCtx      : Error context for reporting constructor errors
    */
   Split(T                    *pIn,
         size_t                inPitchBytes,
         const TISP_BufDesc2D *pOutputs,
         uint32_t              numOutputs,
         uint32_t              inSamples,
         uint8_t               isInterleave,
         ErrorCtxt            &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_split node with a name and ID.
    *
    * This constructor is a delegator that accepts a node name and ID.
    *
    * @param [in]   pIn           : Pointer to the input array.
    * @param [in]   inPitchBytes  : Pitch in bytes for the input array.
    * @param [out]  pOutputs      : Array of per-output descriptors (one entry per output).
    *                               Each entry carries the output buffer pointer, its
    *                               width/height (pre-oriented per isInterleave), and its
    *                               row stride in bytes.
    * @param [in]   numOutputs    : Number of outputs.
    * @param [in]   inSamples     : Samples per channel.
    * @param [in]   isInterleave  : 0=planar, 1=interleaved.
    * @param [in]   nodeName      : Name of the node.
    * @param [in]   nodeId        : ID of the node.
    * @param [in]   errorCtx      : Error context for reporting constructor errors
    */
   Split(T                    *pIn,
         size_t                inPitchBytes,
         const TISP_BufDesc2D *pOutputs,
         uint32_t              numOutputs,
         uint32_t              inSamples,
         uint8_t               isInterleave,
         const char           *nodeName,
         uint32_t              nodeId,
         ErrorCtxt            &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pIn (input array)
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
    * - index 0 to _numOutputs-1: _outputPtrs[index] (output array)
    *
    * @param[in]  index : Index of the output buffer (0 to _numOutputs-1)
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
    * @retval    size_t : The number of output buffers (_numOutputs: _pY)
    * @throw None
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function sets the source and destination addresses for the operation.
    *
    * @param [in] addrs  : Structure containing source and destination addresses.
    * @param [in] mode   : Mode of address setting (input, output, or both).
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
    * @retval const char* The name of the node.
    * @throw None
    */
   const char *getNodeName() const noexcept override { return _nodeInfo.getNodeName(); }

   /**
    * @brief Get the ID of the node
    *
    * This function returns the ID of the node.
    *
    * @retval uint32_t The ID of the node.
    * @throw None
    */
   uint32_t getNodeId() const noexcept override { return _nodeInfo.getNodeId(); }

   /**
    * @brief Get the parameters of the node
    *
    * This function returns a string containing the parameters of the node.
    *
    * @retval const char* The parameters of the node.
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
    * @brief Destructor
    *
    * This destructor is set to default.
    *
    * @retval None
    * @throw None
    */
   ~Split() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP

/* ======================================================================== */
/*  End of file:  TISP_split.hpp                                           */
/* ======================================================================== */
