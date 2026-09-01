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
 * \class Dcsource
 * @brief Provides node-level APIs for the DSPLIB_blkCopyConst2D kernel from DSPLIB.
 * @tparam T : Supports int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double.
 *
 * <br>The kernel supports copying 2D blocks of data with specified dimensions and stride.
 *
 * <b>Example: </b>
 *
 * <b>Input constant value</b> (pC): -8 <br>
 *
 * <b>Output</b> (pY): No. of samples = N; No. of channels = M.
 *  | Channel/Sample |  s0  |  s1  | ... | sN-1 |
 *  |--------------- |------|------|-----|------|
 *  |      ch0       |  -8  |  -8  | ... |  -8  |
 *  |      ch1       |  -8  |  -8  | ... |  -8  |
 *  |      .         |   .  |   .  | ... |   .  |
 *  |      .         |   .  |   .  | ... |   .  |
 *  |      .         |   .  |   .  | ... |   .  |
 *  |     chM-1      |  -8  |  -8  | ... |  -8  |
 *
 */

template <typename T> class Dcsource final : public VirtualOp {

   T *_pC; /*!< Pointer to input constant value. */
   T *_pY; /*!< Pointer to output matrix. */

   size_t _width;
   size_t _height;
   size_t _outPitchBytes;

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level DSPLIB_blkCopyConst2D kernel. */

 public:
   /**
    * @brief Creates a AUDIOLIB_dcSource node.
    *
    * This constructor initializes a node for the DSPLIB_blkCopyConst2D kernel.
    *
    * @param [in]  *pC           : Pointer to input constant value.
    *                              Initializes @ref _pC.
    * @param [out] *pY           : Pointer to the output matrix.
    *                              Initializes @ref _pY.
    * @param [in]  width         : Width of output matrix.
    * @param [in]  height        : Height of output matrix.
    * @param [in]  outPitchBytes : Pitch of output matrix in bytes.
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    *
    */
   Dcsource(T *pC, T *pY, size_t width, size_t height, size_t outPitchBytes, ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_dcSource node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Dcsource(T          *pC,
            T          *pY,
            size_t      width,
            size_t      height,
            size_t      outPitchBytes,
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

   ~Dcsource() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
