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
 * \class Ssrc
 * @brief Provides node-level APIs for the AUDIOLIB_ssrc kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel performs synchronous sample rate conversion for input signals individually by converting
 * audio data between different sample rates. It is optimized for high-performance, multi-channel audio processing.
 *
 * It supports both interleaved and non-interleaved formats of data.
 *
 * <b>Example : </b>
 *
 *  | Input Sample Rate | Output Sample Rate | Input Sample Count | Input Channels | Output Sample Count | Output
 * channels |
 *  |------------------
 * |--------------------|--------------------|----------------|---------------------|-----------------| |       8000 |
 * 16000       |         16         |       1        |         32          |        1        | |      11025        |
 * 22050       |         256        |       1        |         512         |        1        | |      12000        |
 * 24000       |         256        |       1        |         512         |        1        | |      16000        |
 * 32000       |         128        |       1        |         256         |        1        | |      22050        |
 * 44100       |          8         |       1        |         16          |        1        | |      24000        |
 * 48000       |         64         |       1        |         128         |        1        | |      32000        |
 * 64000       |         32         |       1        |         64          |        1        | |      44100        |
 * 88200       |          4         |       1        |          8          |        1        | |      48000        |
 * 96000       |         64         |       1        |         128         |        1        | |      64000        |
 * 128000      |         256        |       1        |         512         |        1        | |      96000        |
 * 192000      |         128        |       1        |         256         |        1        | |       8000        |
 * 32000       |          3         |       1        |         12          |        1        | |      11025        |
 * 44100       |         32         |       1        |         128         |        1        | |      12000        |
 * 48000       |         256        |       1        |         1024        |        1        | |      16000        |
 * 64000       |         32         |       1        |         128         |        1        | |      22050        |
 * 88200       |         16         |       1        |         64          |        1        | |      24000        |
 * 96000       |         64         |       1        |         256         |        1        | |      44100        |
 * 176400      |         127        |       1        |         508         |        1        | |      48000        |
 * 192000      |         15         |       1        |         60          |        1        | |      16000        |
 * 8000       |          4         |       1        |          2          |        1        | |      22050        |
 * 11025       |         32         |       1        |         16          |        1        | |      24000        |
 * 12000       |         64         |       1        |         32          |        1        | |      32000        |
 * 16000       |         32         |       1        |         16          |        1        | |      44100        |
 * 22050       |         16         |       1        |          8          |        1        | |      48000        |
 * 24000       |         64         |       1        |         32          |        1        | |      192000       |
 * 96000       |         256        |       1        |         128         |        1        | |      32000        |
 * 8000       |          4         |       1        |          1          |        1        | |      64000        |
 * 16000       |          8         |       1        |          2          |        1        | |      176400       |
 * 44100       |         256        |       1        |         64          |        1        | |      96000        |
 * 24000       |         32         |       1        |          8          |        1        | |      192000       |
 * 48000       |         100        |       1        |         25          |        1        |
 */

template <typename T> class Ssrc final : public VirtualOp {

   T *_pX;          /*!< Pointer to input matrix. */
   T *_pState;      /*!< Pointer to buffer holding the state data. */
   T *_pFiltCoeffs; /*!< Pointer to the destination buffer where coefficients will be copied. */
   T *_pY;          /*!< Pointer to output matrix. */

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */
   size_t   _stateBufferSize; /*!< Size of state buffer in bytes. */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_ssrc kernel. */

   TISP::HandlePtr<char> _deinterleaveHandle;
   TISP::HandlePtr<char> _interleaveHandle;
   TISP::HandlePtr<char> _blkCopy2DHandle1;
   TISP::HandlePtr<char> _blkCopy2DHandle2;
   TISP::HandlePtr<char> _blkCopy2DHandle3;
   TISP::HandlePtr<char> _firHandle1;
   TISP::HandlePtr<char> _firHandle2;

 public:
   /**
    * @brief Creates an AUDIOLIB_ssrc node.
    *
    * This constructor initializes a node for the AUDIOLIB_ssrc kernel.
    *
    * @param [in]  pX                 : Pointer to the input matrix.
    *                                   Initializes @ref _pX.
    * @param [in]  pState             : Pointer to buffer holding the state data.
    *                                   Initializes @ref _pState.
    * @param [out] pFiltCoeffs        : Pointer to the destination buffer where coefficients will be copied.
    *                                   Initializes @ref _pFiltCoeffs.
    * @param [out] pY                 : Pointer to the output matrix.
    *                                   Initializes @ref _pY.
    * @param [in]  widthIn            : Width of input matrix.
    * @param [in]  heightIn           : Height of input matrix.
    * @param [in]  inPitchBytes       : Pitch of input matrix in bytes.
    * @param [in]  widthOut           : Width of output matrix.
    * @param [in]  heightOut          : Height of output matrix.
    * @param [in]  outPitchBytes      : Pitch of output matrix in bytes.
    * @param [in]  sampleDataType     : Ssrc input sample's data type.
    * @param [in]  inputSampleRate    : Input sample rate. Allowed values are 8 kHz, 11.025 kHz, 12 kHz, 16kHz, 22.05
    * kHz, 24 kHz, 32 kHz, 44.1 kHz, 48 kHz, 64 kHz, 96 kHz, 176.4 kHz, and 192 kHz.
    * @param [in]  outputSampleRate   : Output sample rate. Allowed values are 8 kHz, 11.025 kHz, 12 kHz, 16kHz, 22.05
    * kHz, 24 kHz, 32 kHz, 44.1 kHz, 48 kHz, 64 kHz, 88.2 kHz, 96 kHz, 128 kHz, 176.4 kHz, and 192 kHz, .
    * @param [in]  numChannels        : Number of ssrc channels.
    * @param [in]  isInterleave       : Indicates whether the input matrix data is interleaved:
    *                                 - `0`: Non-interleaved (Rows represent channels, columns represent samples).
    *                                 - `1`: Interleaved (Rows represent samples, columns represent channels).
    * @param [in]  bufferFormat       : Buffer Format.
    *                                 - '0': AUDIOLIB_SSRC_BUFFER_FORMAT_LINEAR.
    *                                 - '1': AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR.
    * @param [in]  enableMMA          : Flag to indicate whether to use MMA unit (1) or not (0).
    * @param [in]  mmaSize            : MMA size related to precision.
    *                                 - AUDIOLIB_SSRC_MODE_RESET: Resets the filter state.
    * @param [in]  inputSampleCount   : Max number of ssrc input samples.
    * @param [in]  errorCtx           : Error context for reporting constructor errors
    *
    */
   Ssrc(T         *pX,
        T         *pState,
        T         *pFiltCoeffs,
        T         *pY,
        size_t     inputSampleCount,
        size_t     sampleDataType,
        size_t     widthIn,
        size_t     heightIn,
        size_t     inPitchBytes,
        size_t     widthOut,
        size_t     heightOut,
        size_t     outPitchBytes,
        size_t     inputSampleRate,
        size_t     outputSampleRate,
        size_t     numChannels,
        size_t     isInterleave,
        size_t     bufferFormat,
        size_t     enableMMA,
        size_t     mmaSize,
        ErrorCtxt &errorCtx);

   /**
    * @brief Creates a AUDIOLIB_ssrc node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */

   Ssrc(T          *pX,
        T          *pState,
        T          *pFiltCoeffs,
        T          *pY,
        size_t      inputSampleCount,
        size_t      sampleDataType,
        size_t      widthIn,
        size_t      heightIn,
        size_t      inPitchBytes,
        size_t      widthOut,
        size_t      heightOut,
        size_t      outPitchBytes,
        size_t      inputSampleRate,
        size_t      outputSampleRate,
        size_t      numChannels,
        size_t      isInterleave,
        size_t      bufferFormat,
        size_t      enableMMA,
        size_t      mmaSize,
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
    * @brief Clean and reset all internal state buffers to zero when data is non-interleaved or when data is interleaved
    * with linear bufferFormat.
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

   ~Ssrc() = default;
};

/** @} */

} /* namespace AUDIOLIB */

} /* namespace TISP */
