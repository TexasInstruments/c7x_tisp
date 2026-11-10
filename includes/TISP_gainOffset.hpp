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
 * \class GainOffset
 * @brief Provides node-level APIs for the AUDIOLIB_gainOffset kernel from AUDIOLIB.
 * @tparam T : Supports float, double and int32_t.
 *
 * @details Multiplies every sample by @c gain, adds @c offset (y = gain * x + offset) and writes the output.
 *          The same gain and offset apply to every channel. Input and output share one layout (interleaved or
 *          non-interleaved) and must be different, non-overlapping buffers. Gain and offset must be finite.
 *
 * @image html gainOffset_node_kernel.svg "GainOffset node block diagram" width=800px
 **/

/**
 * @brief Parameter type of the gain and offset of a @ref GainOffset node with element type @p T.
 *
 * @details `float` for float and double data, `int32_t` for int32 data, as required by AUDIOLIB_gainOffset.
 *
 * @tparam T : Node element type (float, double or int32_t).
 */
template <typename T> struct GainOffsetParamType {
   using type = float; /*!< float for float and double data */
};

template <> struct GainOffsetParamType<int32_t> {
   using type = int32_t; /*!< int32_t for int32 data: plain integers, no fractional bits */
};

/**
 * @brief Construction-time (non-tunable) parameters of the @ref GainOffset node.
 *
 * @details Required; sets the gain and the offset applied to every sample.
 *
 * @tparam T : Node element type (float, double or int32_t).
 */
template <typename T> struct GainOffsetConfig {
   using ParamT = typename GainOffsetParamType<T>::type; /*!< float for float/double data, int32_t for int32 data */

   ParamT gain   = static_cast<ParamT>(1); /*!< Scalar gain applied to every sample. */
   ParamT offset = static_cast<ParamT>(0); /*!< Scalar offset added after the gain. */
};

template <typename T> class GainOffset final : public VirtualOp {

   NodeInfo       _nodeInfo; /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D _in;       /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D _out;      /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_gainOffset kernel */

 public:
   /**
    * @brief Creates an AUDIOLIB_gainOffset node.
    *
    * @details Validates the buffer descriptors and initializes the AUDIOLIB_gainOffset kernel with the gain and
    *          offset from @p pConfig.
    *
    * @param [in] pInputs  : Input buffer descriptors; exactly one buffer. Its @c pBuf is the
    *                        initial input pointer, rebound later by @ref setAddr.
    * @param [in] pOutputs : Output buffer descriptors; exactly one buffer with the same layout as
    *                        the input.
    * @param [in] pConfig  : Construction-time parameters; required.
    * @param [in] nodeName : Name of the node
    * @param [in] nodeId   : ID of the node
    * @param [in] errorCtx : Error context for reporting constructor errors
    *
    * @throw None
    */
   GainOffset(const TISP_BufDescList    *pInputs,
              const TISP_BufDescList    *pOutputs,
              const GainOffsetConfig<T> *pConfig,
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
    * @param [in] addrs : Structure containing source and destination addresses
    * @param [in] mode  : Mode of address setting (input, output, or both)
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
   ~GainOffset() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
