// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_virtualOp.hpp"

#include <cstddef>
#include <cstdint>

#if !defined(HOST_EMULATION)
#include "drivers/dmautils/src/dmautils_autoincrement_3d_priv.h"
#include <drivers/dmautils/dmautils.h>
#include <drivers/udma.h>
#endif

namespace TISP {

namespace misc {

/**
 *  @ingroup miscNode
 */

/**@{*/

/**
 *  \class DelayDru
 *  @brief TISP node that implements a multi-channel per-channel delay line
 *         entirely with DRU circular addressing.  Persistent delay state
 *         (the ring buffer) stays resident in DDR; no C7x compute runs.
 *  @tparam T : Supports float and double.
 *
 *  @details Each frame, the input frame (planar, [numChannels][frameSamples])
 *  is written into a per-channel circular ring in DDR at the current write
 *  offset (one 2-D transfer, circular on the destination side), then each
 *  channel's delayed output is read back out of the ring at
 *  `writeOff - delaySamples[c]` (one 1-D transfer per channel, circular on
 *  the source side).  The DRU's circular addressing hardware performs the
 *  wraparound -- no CPU/C7x arithmetic is needed on the data path.
 *
 *  Both the write-in transfer and every channel's read-out transfer reuse a
 *  single owned DRU channel (id 0), configured/triggered/waited/deconfigured
 *  serially -- mirroring how @ref DMAXfer "DMAXfer<1>" reuses one channel for
 *  all of its transfers.
 *
 *  Unlike @ref DMAXfer, DelayDru does NOT use `dmaUtils::configure_channel()`
 *  (it hardcodes `circSize1=0` / `addrMode=LINEAR`, i.e. linear-only). The
 *  raw `DmaUtilsAutoInc3d_*` driver API is called directly so the write-in
 *  and read-out transfers can use circular addressing.
 *
 *  @par Ring sizing and alignment
 *  `ringSamplesPerCh` is the next power of two >= `frameSamples + maxDelay`
 *  (DRU circular addressing requires a power-of-two circular size).  The
 *  caller-allocated `pRingDdr` buffer must be:
 *    - sized `numChannels * ringBytesPerCh()` bytes (see @ref ringBytes),
 *    - aligned to `ringBytesPerCh` bytes, so that every per-channel block
 *      (`pRingDdr + c * ringBytesPerCh`) is itself aligned to `ringBytesPerCh`
 *      and the DRU's circular address masking wraps correctly within each
 *      channel's block,
 *    - non-cacheable DDR (no CPU cache maintenance is applied to the ring;
 *      only `_pIn` / `_pOut`, which live in cacheable L2SRAM, are flushed).
 *
 *  @par HOST_EMULATION
 *  On PC, the DRU/UDMA driver is unavailable. `exec()` instead runs the
 *  identical ring math on the CPU (same as the PoC's `App_delayReference`),
 *  using the same caller-allocated `pRingDdr` buffer as plain heap memory.
 */
template <typename T> class DelayDru final : public VirtualOp {

   T        *_pIn;            /*!< L2 input frame  [numChannels][frameSamples], planar */
   T        *_pOut;           /*!< L2 output frame [numChannels][frameSamples], planar */
   uint8_t  *_pRingDdr;       /*!< Caller-allocated ring: numChannels * _ringBytesPerCh bytes */
   uint32_t *_delaySamples;   /*!< Owned copy of per-channel delay sizes [numChannels] */

   size_t   _frameSamples;      /*!< Samples per channel per frame */
   size_t   _numChannels;       /*!< Number of channels */
   uint32_t _maxDelay;          /*!< max(_delaySamples[0..numChannels-1]) */
   size_t   _ringSamplesPerCh;  /*!< next_pow2(frameSamples + maxDelay) */
   size_t   _ringBytesPerCh;    /*!< _ringSamplesPerCh * sizeof(T) */
   size_t   _writeOff;          /*!< Current ring write offset, in samples */

   // ── Values that are fixed once frameSamples/numChannels/ring sizing are
   //    known — computed exactly once in the constructor so exec() never
   //    recomputes them. ──
   size_t  *_chRingSampOff;   /*!< Per-channel ring base offset, in samples: [c] = c * _ringSamplesPerCh */
   size_t   _frameBytes;      /*!< frameSamples * sizeof(T) */
   size_t   _frameBytesTotal; /*!< numChannels * _frameBytes */

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

#if !defined(HOST_EMULATION)
   // ── DMA engine resources (target only) — single owned DRU channel (id 0) ──
   uint8_t           *_dmaUtilsContext; /*!< DmaUtils ctx sized for 1 channel */
   uint8_t            *_pTrMem;         /*!< TR memory, reused for write-in + every read-out */
   Udma_DrvHandle       _drvHandle;     /*!< Application-owned UDMA driver handle (not init'd/deinit'd here) */
   int32_t              _chId;          /*!< Owned DRU channel id (always 0) */

   // ── Transfer descriptor templates — built ONCE in the constructor (syncType,
   //    dmaDfmt, circProp, and the fixed transferDim counts/dims never change
   //    frame-to-frame). exec() only refreshes ioPointers.srcPtr/dstPtr (which
   //    move with _writeOff / setAddr) before prepareTr/configure/trigger/wait/
   //    deconfigure -- the driver's TR memory must still be rebuilt whenever
   //    the transfer's addresses move, but no field of these structs is
   //    recomputed per frame. ──
   DmaUtilsAutoInc3d_TrPrepareParam _trPrep; /*!< Fixed: channelId/trMem/trMemSize/numTRs=1 */

   // Write-in: one SYNC_2D TR, icnt0=frameBytes, icnt1=numChannels, circDir=DST,
   // circSize1=ringBytesPerCh. No burst tiling: a bulk+tail split is unsafe under CIRC1
   // because a tail pointer pushed past a channel-ring boundary masks to the next
   // channel's ring. _tpWriteInTail is retained (kept zeroed / sicnt0==0) so exec()'s
   // skip guard stays valid without further #ifdef churn.
   DmaUtilsAutoInc3d_TransferProp _tpWriteInBulk;
   DmaUtilsAutoInc3d_TransferProp _tpWriteInTail; /*!< unused: kept zeroed, sicnt0==0 => skipped in exec() */

   // Read-out: one SYNC_1D TR per channel, icnt0=frameBytes, circDir=SRC. Channel-agnostic
   // (only ioPointers move per channel/frame). No tiling, same CIRC1-safety rationale.
   DmaUtilsAutoInc3d_TransferProp _tpReadOutBulk;
   DmaUtilsAutoInc3d_TransferProp _tpReadOutTail; /*!< unused: kept zeroed, sicnt0==0 => skipped in exec() */

   /*!< Run one prepared descriptor: prepareTr -> configure -> trigger -> wait -> deconfigure
        on the single owned channel (id 0). `tp` must have ioPointers already filled in. */
   void runTr(DmaUtilsAutoInc3d_TransferProp *tp) noexcept;
#endif

 public:
   const char          *getNodeName()     const noexcept override { return _nodeInfo.getNodeName();     }
   uint32_t             getNodeId()       const noexcept override { return _nodeInfo.getNodeId();       }
   const char          *getNodeParams()   const noexcept override { return _nodeInfo.getNodeParams();   }
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Construct a DelayDru<T> node that uses an application-owned UDMA driver.
    *
    * The application must have called @c Udma_init() on the driver object before
    * constructing the node and must @c Udma_deinit() it after the node is destroyed.
    *
    * @param [in,out] pIn           L2 input frame  [numChannels][frameSamples], planar.
    * @param [out]    pOut          L2 output frame [numChannels][frameSamples], planar.
    * @param [in]     pRingDdr      Caller-allocated NON-CACHEABLE DDR ring, sized and
    *                               aligned as documented on the class -- see @ref ringBytes.
    * @param [in]     pDelaySamples Per-channel delay in samples [numChannels].
    * @param [in]     frameSamples  Samples per channel per frame.
    * @param [in]     numChannels   Number of channels.
    * @param [in]     pUdmaDrvHandle Initialised @c Udma_DrvHandle owned by the application
    *                               (ignored, may be nullptr, on HOST_EMULATION).
    * @param [in,out] errorCtx      Error context for reporting constructor errors.
    */
   DelayDru(T         *pIn,
            T         *pOut,
            void      *pRingDdr,
            uint32_t  *pDelaySamples,
            size_t     frameSamples,
            size_t     numChannels,
            void      *pUdmaDrvHandle,
            ErrorCtxt &errorCtx);

   /**
    * @brief Creates a DelayDru node with a name and ID.
    *
    * Convenience overload -- same as the primary constructor, but also takes
    * @c nodeName and @c nodeId.
    */
   DelayDru(T          *pIn,
            T          *pOut,
            void       *pRingDdr,
            uint32_t   *pDelaySamples,
            size_t      frameSamples,
            size_t      numChannels,
            void       *pUdmaDrvHandle,
            const char *nodeName,
            uint32_t    nodeId,
            ErrorCtxt  &errorCtx);

   /**
    * @brief Get the address of an input buffer.
    *
    * Buffer Address Mapping:
    * - index 0: _pIn (input frame) - managed by setAddr()
    *
    * @param[in]  index : Index of the input buffer (0)
    * @retval     void* : Pointer to the input buffer, or nullptr if the index is invalid
    */
   void *getInputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the address of an output buffer.
    *
    * Buffer Address Mapping:
    * - index 0: _pOut (output frame) - managed by setAddr()
    *
    * @param[in]  index : Index of the output buffer (0)
    * @retval     void* : Pointer to the output buffer, or nullptr if the index is invalid
    */
   void *getOutputAddr(size_t index) const noexcept override;

   /**
    * @brief Get the number of input buffers (1: _pIn).
    */
   size_t getNumInputs() const noexcept override;

   /**
    * @brief Get the number of output buffers (1: _pOut).
    */
   size_t getNumOutputs() const noexcept override;

   /**
    * @brief Update _pIn / _pOut from addrs.src[0] / addrs.dst[0].
    *
    * Unlike DMAXfer (whose addresses are fixed at construction), DelayDru
    * keeps the ring write offset live across frames as internal state, so
    * updating the frame addresses via setAddr() between frames is meaningful.
    *
    * @param [in] addrs : Structure containing source and destination addresses
    * @param [in] mode  : Mode of address setting (input, output, or both)
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   /**
    * @brief Execute one frame of the delay: write-in (circular DST) then
    *        one read-out per channel (circular SRC), then advance the ring
    *        write offset.  On HOST_EMULATION, runs the equivalent CPU ring
    *        model instead.
    */
   void exec() noexcept override;

   /**
    * @brief Zero the DDR ring and reset the write offset to 0.
    */
   void cleanInternalBuffers() noexcept override;

   /**
    * @brief Size (in bytes) of the per-channel ring a caller must allocate
    *        for `pRingDdr`, given `frameSamples`, the maximum per-channel
    *        delay, and `numChannels`. The caller must allocate
    *        `numChannels * ringBytes(...)` bytes total, aligned to the
    *        returned size (see class-level documentation).
    *
    * @param frameSamples Samples per channel per frame.
    * @param maxDelay     max(pDelaySamples[0..numChannels-1]).
    * @param numChannels  Number of channels (unused in the per-channel size
    *                     computation; kept for signature symmetry / future use).
    * @retval size_t      Per-channel ring size in bytes: next_pow2(frameSamples + maxDelay) * sizeof(T).
    */
   static size_t ringBytes(size_t frameSamples, uint32_t maxDelay, size_t numChannels);

   ~DelayDru();
};

// Explicit instantiation declarations — definitions are in TISP_delayDru.cpp.
extern template class DelayDru<float>;
extern template class DelayDru<double>;

/** @} */

} // namespace misc

} // namespace TISP
