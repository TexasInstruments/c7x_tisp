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
 * \class Tableinterpolate
 * @brief Provides node-level APIs for the AUDIOLIB_tableInterpolate kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel performs **linear interpolation** on multiple audio channels using
 * a shared lookup table. Each input sample is normalized between the minimum and
 * maximum source values and then used to linearly interpolate between adjacent
 * table entries.
 *
 */

template <typename T> class Tableinterpolate final : public VirtualOp {

   T *_pX; /*!< Pointer to input array. */
   T *_pG; /*!< Pointer to table buffer. */
   T *_pY; /*!< Pointer to output array. */

   size_t   _srcSamples;
   size_t   _tableSamples;
   float    _minVal;
   float    _maxVal;
   uint32_t _tableInterpolateSize;

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_tableInterpolate kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_tableInterpolate node.
    *
    * This constructor initializes a node for the AUDIOLIB_tableInterpolate kernel.
    *
    * @param [in]  *pX                  : Pointer to the input array.
    *                                     Initializes @ref _pX.
    * @param [in]  *pG                  : Pointer to the table buffer.
    *                                     Initializes @ref _pG.
    * @param [out] *pY                  : Pointer to the output array.
    *                                     Initializes @ref _pY.
    * @param [in]  srcsamples           : Width of input array.
    * @param [in]  tableSamples         : Width of table buffer.
    * @param [in]  minVal               : Minimum value of the source index.
    * @param [in]  maxVal               : Maximum value of the source index.
    * @param [in]  tableInterpolateSize : Width of the table buffer.
    * @param [in]  errorCtx             : Error context for reporting constructor errors
    *
    */
   Tableinterpolate(T         *pX,
                    T         *pG,
                    T         *pY,
                    size_t     srcsamples,
                    size_t     tableSamples,
                    float      minVal,
                    float      maxVal,
                    uint32_t   tableInterpolateSize,
                    ErrorCtxt &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_tableInterpolate node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Tableinterpolate(T          *pX,
                    T          *pG,
                    T          *pY,
                    size_t      srcsamples,
                    size_t      tableSamples,
                    float       minVal,
                    float       maxVal,
                    uint32_t    tableInterpolateSize,
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
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~Tableinterpolate() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
