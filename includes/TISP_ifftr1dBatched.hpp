// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "../src/common/TISP_common.hpp"
#include "TISP_ErrorCtxt.hpp"
#include "TISP_common.hpp"
#include <cstdint>
#include <fftlib.h>
#include <memory>

#define TISP_IFFT1DBATCHED_I32FC_C32FC_O32F_PBLOCK_SIZE  FFTLIB_IFFT1DBATCHED_I32FC_C32FC_O32F_PBLOCK_SIZE

namespace TISP {

namespace FFTLIB {

/**
 *  @ingroup fftlibNode
 */

/**@{*/

/**
 * @brief Construction-time (non-tunable) parameters of the @ref IFFTR1dBatched node.
 *
 * @details Required; selects the input spectrum.
 */
struct IFFTR1dBatchedConfig {
   bool fullSpectrum = false; /*!< `true`: `numPoints` input bins, `false`: `numPoints / 2 + 1` bins. */
};

/**
 *  \class IFFTR1dBatched
 *  @brief Provides node-level APIs for the FFTLIB_IFFTR1dBatched kernel from FFTLIB. Assumes complex float data that is
 *         stored in interleaved fashion.
 *  @tparam T        : Supports float
 *
 *  @details Computes the real inverse FFT of every channel (row); each input row holds the complex bins as
 *           interleaved real/imaginary pairs.
 */

template <typename T> class IFFTR1dBatched final : public VirtualOp {

   NodeInfo             _nodeInfo;     /*!< Node name, parameters, buffer params, and ID */
   TISP_BufDesc2D       _in;           /*!< Input buffer descriptor; @ref setAddr rebinds its pBuf */
   TISP_BufDesc2D       _out;          /*!< Output buffer descriptor; @ref setAddr rebinds its pBuf */
   T                   *_scratch;      /*!< Scratch buffer holding the intermediate complex IFFT */
   std::shared_ptr<T>   _twiddles;     /*!< Shared FFT twiddle factors (via TwiddleLUT) */
   std::shared_ptr<T>   _splitFactors; /*!< Shared split factors (via TwiddleLUT) */
   FFTLIB_bufParams1D_t _bufParamsX;   /*!< FFTLIB bufParams of the kernel buffers */
   uint32_t             _numPoints;    /*!< FFT size */
   uint32_t             _numChannels;  /*!< Number of channels */
   bool                 _fullSpectrum; /*!< `true`: full spectrum input, `false`: half spectrum */

   TISP::HandlePtr<uint8_t> _block; /*!< SE and SA params of the FFTLIB kernel */

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
    * @brief Creates an FFTLIB_ifft1dBatched_i32fc_c32fc_o32f node.
    *
    * @details Validates the buffer descriptors and initializes the FFTLIB kernel. The FFT size is the output width
    *          and the channel count is its height.
    *
    * @param [in] pInputs      : Input buffer descriptors; exactly one non-interleaved buffer with packed rows
    *                           of 2 x (number of bins) values. Its @c pBuf is rebound later by @ref setAddr.
    * @param [in] pOutputs     : Output buffer descriptors; exactly one non-interleaved real buffer with packed
    *                           rows and the input's height.
    * @param [in] pScratch     : Scratch buffer; required.
    * @param [in] scratchBytes : Size of @c pScratch in bytes; at least output width x height elements.
    * @param [in] pConfig : Construction-time parameters; required.
    * @param [in] nodeName     : Name of the node
    * @param [in] nodeId       : ID of the node
    * @param [in] errorCtx     : Error context for reporting constructor errors
    *
    * @throw None
    */
   IFFTR1dBatched(const TISP_BufDescList          *pInputs,
                  const TISP_BufDescList          *pOutputs,
                  void                            *pScratch,
                  size_t                           scratchBytes,
                  const IFFTR1dBatchedConfig *pConfig,
                  const char                      *nodeName,
                  uint32_t                         nodeId,
                  ErrorCtxt                       &errorCtx);

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
    * This destructor is set to default.
    */
   ~IFFTR1dBatched() = default;
};

/** @} */

} // namespace FFTLIB

} // namespace TISP
