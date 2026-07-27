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
 * \class Whitenoise
 * @brief Provides node-level APIs for the AUDIOLIB_whiteNoise kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel creates a frame of pseudo-random white noise by using a 32-bit Linear Congruential Generator
 * (LCG). The LCG generates the next random value based on the current value and three constants a, c and m.
 *
 * The values used by the kernel are:
 * - \f$ a = 1664525 \f$
 * - \f$ c = 1013904223 \f$
 * - \f$ m = 2^{32} \f$
 *
 * The 32-bit integer value normalized by first converting into a floating-point number within the range [0.0, 1.0) by
 * dividing it by 2^{32}.
 *
 * Then, the normalized value is scaled to fit within the desired output range.
 *
 */

template <typename T> class Whitenoise final : public VirtualOp {

   T *_pY; /*!< Pointer to output matrix. */

   size_t   _width;
   float    _range;
   uint32_t _seed;

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_whiteNoise kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_whiteNoise node.
    *
    * This constructor initializes a node for the AUDIOLIB_whiteNoise kernel.
    *
    * @param [out] *pY    : Pointer to the output matrix.
    *                       Initializes @ref _pY.
    * @param [in]  width  : Width of output matrix.
    * @param [in]  range  : The output range. Noise will be in [-range, +range].
    * @param [in]  seed   : The initial 32-bit seed for the LCG.
    * @param [in]  errorCtx  : Error context for reporting constructor errors
    *
    */
   Whitenoise(T *pY, size_t width, float range, uint32_t seed, ErrorCtxt &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_whiteNoise node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Whitenoise(T          *pY,
              size_t      width,
              float       range,
              uint32_t    seed,
              const char *nodeName,
              uint32_t    nodeId,
              ErrorCtxt  &errorCtx);

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
    * @brief Get the address of an output buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pY (output data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

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
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~Whitenoise() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
