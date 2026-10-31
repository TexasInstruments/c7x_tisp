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
 * \class Sinusoidgenerator
 * @brief Provides node-level APIs for the AUDIOLIB_sinusoidGenerator kernel from AUDIOLIB.
 * @tparam T : Supports float.
 *
 * @details This kernel generates an output signal \f$y[n]\f$ by calculating the sine
 * of a phase \f$\phi[n]\f$:
 * \f[y[n] = \sin(\phi[n])\f]
 *
 * The phase \f$\phi[n]\f$ is accumulated at each sample by a phase increment value,
 * \f$\Delta\phi[n]\f$, which determines the instantaneous frequency \f$f[n]\f$.
 * The phase wraps around \f$2\pi\f$ to remain in range.
 * \f[\phi[n] = (\phi[n-1] + \Delta\phi[n]) \pmod{2\pi}\f]
 *
 * The phase increment \f$\Delta\phi[n]\f$ is calculated from the frequency \f$f[n]\f$
 * and the sampling rate \f$f_s\f$:
 * \f[\Delta\phi[n] = \frac{2 \pi f[n]}{f_s}\f]
 *
 * The kernel supports smooth transitions between a `startFrequency` and a
 * `targetFrequency` over a specified `smoothingTime`.
 *
 */

template <typename T> class Sinusoidgenerator final : public VirtualOp {

   T *_pY; /*!< Pointer to output matrix. */

   size_t   _width;
   float    _startPhase;
   float    _smoothingTime;
   int32_t  _samplingRate;
   uint32_t _executionMode;
   float    _startFrequency;
   float    _targetFrequency;
   uint32_t _dataType;

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

   TISP::HandlePtr<char> _handle; /*!< Handle to low level AUDIOLIB_sinusoidGenerator kernel. */

 public:
   /**
    * @brief Creates an AUDIOLIB_sinusoidGenerator node.
    *
    * This constructor initializes a node for the AUDIOLIB_sinusoidGenerator kernel.
    *
    * @param [out] pY              : Pointer to the output matrix.
    *                                Initializes @ref _pY.
    * @param [in]  width           : Width of output matrix.
    * @param [in]  startPhase      : The phase at which the signal starts (in degrees).
    * @param [in]  smoothingTime   : The time to transition from start to target frequency (in milliseconds). 0 for
    * instant change.
    * @param [in]  samplingRate    : The audio sampling rate in Hz (e.g., 48000).
    * @param [in]  executionMode   : Execution mode (e.g., scalar, vector). 0 for scalar, 1 for vector.
    * @param [in]  startFrequency  : The frequency of the signal at the start (in Hz).
    * @param [in]  targetFrequency : The target frequency for the signal to transition to (in Hz).
    * @param [in]  dataType        : The precision of the signal (e.g., AUDIOLIB_FLOAT32).
    * @param [in]  errorCtx        : Error context for reporting constructor errors
    *
    */
   Sinusoidgenerator(T         *pY,
                     size_t     width,
                     float      startPhase,
                     float      smoothingTime,
                     int32_t    samplingRate,
                     uint32_t   executionMode,
                     float      startFrequency,
                     float      targetFrequency,
                     uint32_t   dataType,
                     ErrorCtxt &errorCtx);

   /**
    * @brief Creates an AUDIOLIB_sinusoidGenerator node with a name and ID
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   Sinusoidgenerator(T          *pY,
                     size_t      width,
                     float       startPhase,
                     float       smoothingTime,
                     int32_t     samplingRate,
                     uint32_t    executionMode,
                     float       startFrequency,
                     float       targetFrequency,
                     uint32_t    dataType,
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

   ~Sinusoidgenerator() = default;
};

/** @} */

} // namespace AUDIOLIB

} // namespace TISP
