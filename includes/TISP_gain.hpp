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
 * \class Gain
 * @brief Provides node-level APIs for the AUDIOLIB_gain kernel from AUDIOLIB.
 * @tparam T : Supports float and double.
 *
 * @details This kernel applies a gain factor to each sample in the input buffer _pX
 * and writes the resulting values to the output buffer _pY.
 *
 * <b>Example: </b>
 *
 * <b>Input</b> (_pX): No. of samples = N; No. of channels = M.
 *  | Channel/Sample | s0  | s1  | ... | sN-1  |
 *  |--------------- |-----|-----|-----|-------|
 *  |      ch0       | 100 | 320 | ... |  701  |
 *  |      ch1       | 120 | 220 | ... |  620  |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |      .         |  .  |  .  | ... |   .   |
 *  |     chM-1      | 980 |1050 | ... |  156  |
 *
 * <b>gain value</b> (_pG): 2 <br>
 *
 * <b>Output</b> (_pY): No. of samples = N; No. of channels = M.
 *  | Channel/Sample |  s0   |  s1   | ... |  sN-1  |
 *  |--------------- |-------|-------|-----|--------|
 *  |      ch0       |  200  |  640  | ... |  1402  |
 *  |      ch1       |  240  |  440  | ... |  1240  |
 *  |      .         |   .   |   .   | ... |    .   |
 *  |      .         |   .   |   .   | ... |    .   |
 *  |      .         |   .   |   .   | ... |    .   |
 *  |     chM-1      |  1960 |  2100 | ... |  312   |
 *
 */

template <typename T> class Gain final : public VirtualOp {

   T       *_pX;       /*!< Pointer to input matrix. */
   T       *_pG;       /*!< Pointer to input gain value. */
   T       *_pY;       /*!< Pointer to output matrix. */
   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   size_t _width;
   size_t _height;
   size_t _inPitchBytes;
   size_t _outPitchBytes;

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_gain kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_gain node.
    *
    * This constructor initializes a node for the AUDIOLIB_gain kernel.
    *
    * @param [in]  *pX           : Pointer to the input matrix.
    *                              Initializes @ref _pX.
    * @param [in]  *pG           : Pointer to the input gain value.
    *                              Initializes @ref _pG.
    * @param [out] *pY           : Pointer to the output matrix.
    *                              Initializes @ref _pY.
    * @param [in]  width         : Width of input matrix.
    * @param [in]  height        : Height of input matrix.
    * @param [in]  inPitchBytes  : Pitch of input matrix in bytes.
    * @param [in]  outPitchBytes : Pitch of output matrix in bytes.
    * @param [in]  errorCtx      : Error context for reporting constructor errors
    *
    */
   Gain(T         *pX,
        T         *pG,
        T         *pY,
        size_t     width,
        size_t     height,
        size_t     inPitchBytes,
        size_t     outPitchBytes,
        ErrorCtxt &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_gain node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Gain(T          *pX,
        T          *pG,
        T          *pY,
        size_t      width,
        size_t      height,
        size_t      inPitchBytes,
        size_t      outPitchBytes,
        const char *nodeName,
        uint32_t    nodeId,
        ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer
    *
    * Buffer Address Mapping:
    * - index 0: _pX (input data matrix) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

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

   ~Gain() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
