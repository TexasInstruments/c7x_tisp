// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <cstddef>

namespace TISP {

namespace AUDIOLIB {

/**
 *  @ingroup audiolibNode
 */

/**@{*/

/**
 * \class Asrc
 * @brief Provides node-level APIs for the AUDIOLIB_asrc kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel performs asynchronous sample rate conversion for input signals individually by converting
 * audio data between different sample rates. It is optimized for high-performance, multi-channel audio processing.
 *
 * It supports both interleaved and non-interleaved formats of data.
 *
 * <b>Example : </b>
 *
 *  | Input Sample Rate | Output Sample Rate | Input Sample Count | Input Channels | Output Sample Count | Output
 * channels |
 *  |------------------
 * |--------------------|--------------------|----------------|---------------------|-----------------| |      32000 |
 * 32000       |         31         |       1        |         64          |        1        | |      32000        |
 * 44100       |         256        |       1        |         385         |        1        | |      32000        |
 * 48000       |         256        |       1        |         416         |        1        | |      44100        |
 * 32000       |         64         |       1        |         79          |        1        | |      44100        |
 * 44100       |         256        |       1        |         288         |        1        | |      44100        |
 * 48000       |         256        |       1        |         311         |        1        | |      48000        |
 * 32000       |         256        |       1        |         203         |        1        | |      48000        |
 * 44100       |         256        |       1        |         268         |        1        | |      48000        |
 * 48000       |         256        |       1        |         288         |        1        |
 *
 * <b>Configuration & Parameters:</b>
 *
 * | Parameter      | Configuration Time | Description |
 * |----------------|--------------------|------------------------------------------------------------------------------|
 * | mode, ratio    | Runtime            | Asrc operating mode and ratio. <b>ONLY</b> runtime changeable parameter. |
 *
 * <b>Runtime Control:</b>
 * The mode and asrcRatio are the only parameters that can be changed at runtime.
 * To update them, use `setParams()` with the payload format specified below.
 * Use `getParams()` to retrieve the current state.
 *
 *
 */

typedef struct TISP_asrcMode_t {
   double  asrcRatio;
   uint8_t mode;
} TISP_asrcMode;

/**
 * <b>Runtime Control Philosophy:</b>
 * The Asrc node supports dynamic parameter updates via a robust runtime API. Users can modify
 * specific algorithmic parameters without needing to reconstruct the node.
 *
 */
enum class AsrcParamId : uint16_t {
   kMode = 1 /**< kMode: Asrc operating mode and ratio to set/get */
};

template <typename T> class Asrc final : public VirtualOp {

   T       *_pX;                  /*!< Pointer to input matrix. */
   NodeInfo _nodeInfo;            /*!< Node name, parameters, and ID */
   T       *_pNonInterleavedData; /*!< Pointer to buffer holding the intermediate non interleaved data. */
   T       *_pFiltCoeffs;         /*!< Pointer to buffer holding polyphase filter cefficients. */
   T       *_pFilterRembuf;       /*!< Pointer to buffer holding the filter remaining buffer. */
   T       *_pY;                  /*!< Pointer to output matrix. */

   uint32_t _inputSampleCount;
   uint32_t _outputSampleCount;
   size_t   _nonInterleavedDataBufSize; /*!< Size of non interleaved data buffer in bytes. */

   TISP::HandlePtr<char> _handle;               /*!< Handle to low level AUDIOLIB_asrc kernel. */
   TISP::HandlePtr<char> _matTransHandle;       /*!< Handle to low level DSPLIB_matTrans kernel. */

   TISP_asrcMode _asrcConfig{}; /*!< Cached ASRC state for getParams queries. */

 public:
   /**
    * @brief Creates an AUDIOLIB_asrc node.
    *
    * This constructor initializes a node for the AUDIOLIB_asrc kernel.
    *
    * @param [in]  *pX                    : Pointer to the input matrix.
    *                                       Initializes @ref _pX.
    * @param [in]  *pNonInterleavedData   : Pointer to buffer holding the intermediate non interleaved data.
    *                                       Initializes @ref _pNonInterleavedData.
    * @param [in] *pFiltCoeffs            : Pointer to buffer holding polyphase filter cefficients.
    *                                       Initializes @ref _pFiltCoeffs.
    * @param [in] *pFilterRembuf          : Pointer to buffer holding the filter remaining buffer.
    *                                       Initializes @ref _pFilterRembuf.
    * @param [out] *pY                    : Pointer to the output matrix.
    *                                       Initializes @ref _pY.
    * @param [in]  widthIn                : Width of input matrix.
    * @param [in]  heightIn               : Height of input matrix.
    * @param [in]  inPitchBytes           : Pitch of input matrix in bytes.
    * @param [in]  widthOut               : Width of output matrix.
    * @param [in]  heightOut              : Height of output matrix.
    * @param [in]  outPitchBytes          : Pitch of output matrix in bytes.
    * @param [in]  maxSampleCountPerBlock : Max number of asrc input samples per data block.
    * @param [in]  outputSampleCount      : Number of output samples per data block.
    * @param [in]  sampleDataType         : Asrc input sample's data type.
    * @param [in]  inputSampleRate        : Input sample rate. Allowed values are 32kHz, 44.1 kHz, and 48 kHz.
    * @param [in]  outputSampleRate       : Output sample rate. Allowed values are 48 kHz.
    * @param [in]  numChannels            : Number of asrc channels.
    * @param [in]  frameModuloFactor      : Frame modulo factor, used to calculate the output sample count.
    *                                       This is used to ensure that the output sample count is a multiple of this
    * factor.
    * @param [in]  isInterleave           : Indicates whether the input matrix data is interleaved:
    *                                     - `0`: Non-interleaved (Rows represent channels, columns represent samples).
    *                                     - `1`: Interleaved (Rows represent samples, columns represent channels).
    * @param [in]  errorCtx               : Error context for reporting constructor errors
    *
    */
   Asrc(T         *pX,
        T         *pNonInterleavedData,
        T         *pFiltCoeffs,
        T         *pFilterRembuf,
        T         *pY,
        size_t     widthIn,
        size_t     heightIn,
        size_t     inPitchBytes,
        size_t     widthOut,
        size_t     heightOut,
        size_t     outPitchBytes,
        size_t     maxSampleCountPerBlock,
        size_t     outputSampleCount,
        size_t     sampleDataType,
        size_t     inputSampleRate,
        size_t     outputSampleRate,
        size_t     numChannels,
        size_t     frameModuloFactor,
        size_t     isInterleave,
        ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_asrc node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */

   Asrc(T          *pX,
        T          *pNonInterleavedData,
        T          *pFiltCoeffs,
        T          *pFilterRembuf,
        T          *pY,
        size_t      widthIn,
        size_t      heightIn,
        size_t      inPitchBytes,
        size_t      widthOut,
        size_t      heightOut,
        size_t      outPitchBytes,
        size_t      maxSampleCountPerBlock,
        size_t      outputSampleCount,
        size_t      sampleDataType,
        size_t      inputSampleRate,
        size_t      outputSampleRate,
        size_t      numChannels,
        size_t      frameModuloFactor,
        size_t      isInterleave,
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
    * @brief Set the output sample count
    *
    * This function required to set the outputSampleCount varibale to send the number of output samples back to user
    *
    * @retval uint32_t The parameters of the node
    */
   size_t getNumOutputSampleCount() const noexcept;

   /**
    * @brief Update runtime parameters with detailed error reporting
    *
    * Updates the specified runtime parameter by ID with the data provided in the value buffer.
    * The node strictly validates that the provided `size` argument matches or exceeds the
    * expected byte size for the given parameter before mapping the state into the active node memory.
    *
    * @param [in] paramId      : Parameter ID to update
    * @param [in] value        : Pointer to a buffer containing the new parameter value
    * @param [in] size         : The size in bytes of the payload
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true         : Parameters updated successfully
    * @retval false        : Failed to update parameters
    */
   bool setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept override;

   /**
    * @brief Retrieve current runtime parameters with detailed error reporting
    *
    * This API fetches the current live state from the node for the specified parameter ID
    * and writes it into the provided buffer.
    *
    * If the user provides a buffer, it populates the requested data. If the buffer is too small
    * (or `value` is `nullptr`), it safely fails (returns `false`) but injects the exact required
    * byte capacity into `*size`, allowing the application to re-allocate precisely.
    *
    * @param [in] paramId      : Parameter ID to query
    * @param [out] value       : Pointer to a buffer where parameters will be stored
    * @param [in,out] size     : Input: Capacity of value buffer in bytes. Output: Actual bytes written or required.
    * @param [in,out] errorCtx : Error context for reporting errors
    *
    * @retval true          : Parameters retrieved successfully
    * @retval false         : Failed to retrieve parameters
    */
   bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept override;

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
    * @brief Clean and reset all internal history buffers to zero when the data is interleaved.
    *
    * @retval None
    * @throw None
    */
   void cleanInternalBuffers() noexcept override;

   /**
    * @brief Destructor
    *
    * This destructor is set to default
    *
    */

   ~Asrc() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
