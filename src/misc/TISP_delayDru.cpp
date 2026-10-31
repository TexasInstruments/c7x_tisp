// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_bufDesc.hpp"
#include "../common/TISP_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_delayDru.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if !defined(HOST_EMULATION)
#include "../superNode/TISP_dmaUtils.hpp"
#include <kernel/dpl/CacheP.h>
#endif

namespace TISP {

namespace misc {

// ─────────────────────────────────────────────────────────────────────────────
// File-scope helper: smallest power of two >= v (v >= 1). DRU circular
// addressing requires a power-of-two circular size (see DmaUtilsAutoInc3d_
// TransferCirc::circSize1 docs).
// ─────────────────────────────────────────────────────────────────────────────

static size_t nextPow2(size_t v)
{
   size_t p = 1U;
   while (p < v) {
      p <<= 1U;
   }
   return p;
}


// ─────────────────────────────────────────────────────────────────────────────
// ringBytes — static helper for callers to size pRingDdr.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T> size_t DelayDru<T>::ringBytes(size_t frameSamples, uint32_t maxDelay, size_t numChannels)
{
   (void) numChannels; // per-channel size does not depend on numChannels
   const size_t ringSamplesPerCh = nextPow2(frameSamples + static_cast<size_t>(maxDelay));
   return ringSamplesPerCh * sizeof(T);
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor — uses an application-owned UDMA driver handle. ALL one-time
// setup lives here: DmaUtils init, TR memory allocation, AND the burst-shaped circular TransferProp
// templates (write-in bulk/tail, read-out bulk/tail). None of this is
// recomputed in exec() -- only the moving src/dst pointers are refreshed
// there, per frame.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
DelayDru<T>::DelayDru(const TISP_BufDescList *pInputs,
                      const TISP_BufDescList *pOutputs,
                      const TISP_BufDescList *pStates,
                      const DelayDruConfig   *pConfig,
                      const char             *nodeName,
                      uint32_t                nodeId,
                      ErrorCtxt              &errorCtx)
    : _in{}, _out{}, _ring{}, _frameSamples(0), _numChannels(0), _maxDelay(0),
      _ringSamplesPerCh(0), _ringBytesPerCh(0), _writeOff(0), _frameBytes(0), _frameBytesTotal(0)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   if (pConfig == nullptr) {
      errorCtx.setNullPointerError("DelayDru constructor", "config pointer pConfig is null");
   }
   else if (pConfig->pDelaySamples == nullptr) {
      errorCtx.setNullPointerError("DelayDru constructor", "delay size pointer pConfig->pDelaySamples is null");
   }
#if !defined(HOST_EMULATION)
   else if (pConfig->pUdmaDrvHandle == nullptr) {
      errorCtx.setNullPointerError("DelayDru constructor", "UDMA driver handle pConfig->pUdmaDrvHandle is null");
   }
#endif
   else {
      /* Nothing to do here */
   }

   size_t frameSamples = 0U;
   size_t numChannels  = 0U;

   // Buffer checks (skipped once an error is set): one input, one output, and one ring
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "DelayDru constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "DelayDru constructor (outputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pStates, 1U, 1U, "DelayDru constructor (states)", errorCtx)) {
      const TISP_BufDesc2D &in    = pInputs->pDesc[0];
      const TISP_BufDesc2D &out   = pOutputs->pDesc[0];
      const TISP_BufDesc2D &state = pStates->pDesc[0];

      frameSamples = in.width;
      numChannels  = in.height;

      if ((in.isInterleave != 0U) || (out.isInterleave != 0U)) {
         errorCtx.setInvalidParameterError("DelayDru constructor", "input and output must be non-interleaved");
      }
      else if ((out.width != in.width) || (out.height != in.height)) {
         errorCtx.setInvalidParameterError("DelayDru constructor", "input and output width and height must match");
      }
      else if ((in.pitchBytes != (in.width * sizeof(T))) || (out.pitchBytes != (out.width * sizeof(T)))) {
         errorCtx.setInvalidParameterError("DelayDru constructor", "input and output rows must be packed");
      }
      else if (state.height < numChannels) {
         errorCtx.setInvalidParameterError("DelayDru constructor", "ring state must have one row per channel");
      }
      else {
         /* Nothing to do here */
      }

      if (errorCtx.isSuccess()) {
         _in           = in;
         _out          = out;
         _ring         = state;
         _frameSamples = frameSamples;
         _numChannels  = numChannels;

         for (size_t ch = 0; ch < numChannels; ch++) {
            if (pConfig->pDelaySamples[ch] > _maxDelay) { // NOLINT(MISRA.PTR.ARITH)
               _maxDelay = pConfig->pDelaySamples[ch];    // NOLINT(MISRA.PTR.ARITH)
            }
         }
         if (state.pitchBytes != ringBytes(frameSamples, _maxDelay, numChannels)) {
            errorCtx.setInvalidParameterError("DelayDru constructor",
                                              "ring state pitchBytes must equal ringBytes(frameSamples, maxDelay)");
         }
      }
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      _delaySamples = std::make_unique<uint32_t[]>(numChannels);
      for (size_t ch = 0; ch < numChannels; ch++) {
         _delaySamples[ch] = pConfig->pDelaySamples[ch]; // NOLINT(MISRA.PTR.ARITH)
      }

      _ringSamplesPerCh = nextPow2(frameSamples + static_cast<size_t>(_maxDelay));
      _ringBytesPerCh   = _ringSamplesPerCh * sizeof(T);
      _frameBytes       = frameSamples * sizeof(T);
      _frameBytesTotal  = numChannels * _frameBytes;

      // Per-channel ring base offset (in samples) -- fixed once ring sizing/numChannels
      // are known, so exec() never recomputes c * _ringSamplesPerCh.
      _chRingSampOff = std::make_unique<size_t[]>(numChannels);
      for (size_t c = 0; c < numChannels; c++) {
         _chRingSampOff[c] = c * _ringSamplesPerCh;
      }

      // Initialize _params with the node parameters.
      // Mirrors DelayNChannel's "width=,height=,widthD=,heightD=" schema so a host
      // GUI that queried a delay_n_channel node (and now sees it DRU-offloaded)
      // continues to parse the same keys. width/height describe the per-frame I/O
      // buffer; widthD/heightD describe the delay (ring) buffer.
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(),
                      "width=%zu,height=%zu,widthD=%zu,heightD=%zu",
                      frameSamples, numChannels, _ringSamplesPerCh, numChannels);

      // The DDR ring is internal/owned state, not exposed via getInputAddr()/getOutputAddr(),
      // so only the primary frame buffer pIn/pOut is represented here.
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

#if !defined(HOST_EMULATION)
      _chId            = 0;
      _dmaUtilsContext = AlignedBuffer(static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getContextSize(1))));
      _pTrMem          = AlignedBuffer(static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getTrMemReq(1))));
      _pTrMemRead      = AlignedBuffer(static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getTrMemReq(1))));

      // UDMA driver is initialised and owned by the application.
      _drvHandle = static_cast<Udma_DrvHandle>(pConfig->pUdmaDrvHandle);
      uint32_t retVal = UDMA_SOK;

      // Initialise the DmaUtils context for the single channel. TRs themselves
      // are built directly against the raw DmaUtilsAutoInc3d_* API (below) — NOT via
      // dmaUtils<T>::configure_channel(), which hardcodes circSize1=0 / addrMode=LINEAR
      // and therefore cannot express circular addressing.
      if (_dmaUtilsContext == nullptr || _pTrMem == nullptr || _pTrMemRead == nullptr) {
         errorCtx.setLibraryError("DelayDru constructor", "memalign failed for DmaUtils buffers", 0);
      }

      if (errorCtx.isSuccess()) {
         retVal = dmaUtils<uint8_t>::init(_dmaUtilsContext.get(), 1, _drvHandle);
         if (retVal != UDMA_SOK) {
            errorCtx.setLibraryError("DelayDru constructor", "DmaUtils init failed", static_cast<int32_t>(retVal));
         } else {
            _dmaUtilsInitDone = true; // signals destructor to call deinit
         }
      }

      // Fixed TR-prepare params — channel/trMem/trMemSize/numTRs never change.
      _trPrep.channelId = _chId;
      _trPrep.numTRs    = 1;
      _trPrep.trMem     = _pTrMem.get();
      _trPrep.trMemSize = DmaUtilsAutoInc3d_getTrMemReq(1);

      // ---- Write-in: one SYNC_2D per frame — icnt0=frameBytes, icnt1=numChannels.
      // Mirrors PoC App_setupWriteTr: NO burst tiling. A tiled (bulk+tail) plan is
      // unsafe under CIRC1 because the tail's dstPtr can be pushed past a channel-ring
      // boundary (writeByteOff + bulkBytes >= ringBytesPerCh), so the DRU's
      // (addr & ~(circSize1-1)) wrap base lands in the NEXT channel's ring and corrupts
      // it. With icnt0=frameBytes the only moving pointer is ring+writeByteOff and the
      // channel stride ddim1=ringBytesPerCh is an exact multiple of circSize1, so every
      // channel's wrap base stays within its own ring for any writeOff.
      {
         std::memset(&_tpWriteInBulk, 0, sizeof(_tpWriteInBulk));
         _tpWriteInBulk.syncType = DMAUTILSAUTOINC3D_SYNC_2D; // 2 active dims: icnt0,icnt1
         _tpWriteInBulk.dmaDfmt  = DMAUTILSAUTOINC3D_DFMT_NONE;

         _tpWriteInBulk.circProp.circDir       = DMAUTILSAUTOINC3D_CIRCDIR_DST;
         _tpWriteInBulk.circProp.circSize1     = static_cast<uint64_t>(_ringBytesPerCh);
         _tpWriteInBulk.circProp.circSize2     = 0;
         _tpWriteInBulk.circProp.addrModeIcnt0 = DMAUTILSAUTOINC3D_ADDR_CIRC1;
         _tpWriteInBulk.circProp.addrModeIcnt1 = DMAUTILSAUTOINC3D_ADDR_LINEAR;
         _tpWriteInBulk.circProp.addrModeIcnt2 = DMAUTILSAUTOINC3D_ADDR_LINEAR;
         _tpWriteInBulk.circProp.addrModeIcnt3 = DMAUTILSAUTOINC3D_ADDR_LINEAR;

         _tpWriteInBulk.transferDim.sicnt0 = static_cast<uint16_t>(_frameBytes);
         _tpWriteInBulk.transferDim.sicnt1 = static_cast<uint16_t>(numChannels);
         _tpWriteInBulk.transferDim.sicnt2 = 1U;
         _tpWriteInBulk.transferDim.sicnt3 = 1U;
         _tpWriteInBulk.transferDim.sdim1  = static_cast<int32_t>(_frameBytes);      // channel stride on src (planar, contiguous)
         _tpWriteInBulk.transferDim.sdim2  = 0;
         _tpWriteInBulk.transferDim.sdim3  = 0;

         _tpWriteInBulk.transferDim.dicnt0 = static_cast<uint16_t>(_frameBytes);
         _tpWriteInBulk.transferDim.dicnt1 = static_cast<uint16_t>(numChannels);
         _tpWriteInBulk.transferDim.dicnt2 = 1U;
         _tpWriteInBulk.transferDim.dicnt3 = 1U;
         _tpWriteInBulk.transferDim.ddim1  = static_cast<int32_t>(_ringBytesPerCh);  // channel stride on dst (ring blocks)
         _tpWriteInBulk.transferDim.ddim2  = 0;
         _tpWriteInBulk.transferDim.ddim3  = 0;

         std::memset(&_tpWriteInTail, 0, sizeof(_tpWriteInTail)); // unused; sicnt0==0 skips it in exec()
      }

      // ---- Read-out: one SYNC_1D per channel per frame — icnt0=frameBytes, no tiling.
      // Mirrors PoC App_setupReadTrCh. Same rationale as write-in: no bulk+tail split,
      // so the CIRC1 wrap base stays within the target channel's ring even when
      // readByteOff sits near the ring end. Channel-agnostic (only ioPointers differ
      // per channel), so this single template is reused for every channel, every frame.
      {
         std::memset(&_tpReadOutBulk, 0, sizeof(_tpReadOutBulk));
         _tpReadOutBulk.syncType = DMAUTILSAUTOINC3D_SYNC_1D; // 1 active dim: icnt0
         _tpReadOutBulk.dmaDfmt  = DMAUTILSAUTOINC3D_DFMT_NONE;

         _tpReadOutBulk.circProp.circDir       = DMAUTILSAUTOINC3D_CIRCDIR_SRC;
         _tpReadOutBulk.circProp.circSize1     = static_cast<uint64_t>(_ringBytesPerCh);
         _tpReadOutBulk.circProp.circSize2     = 0;
         _tpReadOutBulk.circProp.addrModeIcnt0 = DMAUTILSAUTOINC3D_ADDR_CIRC1;
         _tpReadOutBulk.circProp.addrModeIcnt1 = DMAUTILSAUTOINC3D_ADDR_LINEAR;
         _tpReadOutBulk.circProp.addrModeIcnt2 = DMAUTILSAUTOINC3D_ADDR_LINEAR;
         _tpReadOutBulk.circProp.addrModeIcnt3 = DMAUTILSAUTOINC3D_ADDR_LINEAR;

         _tpReadOutBulk.transferDim.sicnt0 = static_cast<uint16_t>(_frameBytes);
         _tpReadOutBulk.transferDim.sicnt1 = 1U;
         _tpReadOutBulk.transferDim.sicnt2 = 1U;
         _tpReadOutBulk.transferDim.sicnt3 = 1U;
         _tpReadOutBulk.transferDim.sdim1  = 0;
         _tpReadOutBulk.transferDim.sdim2  = 0;
         _tpReadOutBulk.transferDim.sdim3  = 0;

         _tpReadOutBulk.transferDim.dicnt0 = static_cast<uint16_t>(_frameBytes);
         _tpReadOutBulk.transferDim.dicnt1 = 1U;
         _tpReadOutBulk.transferDim.dicnt2 = 1U;
         _tpReadOutBulk.transferDim.dicnt3 = 1U;
         _tpReadOutBulk.transferDim.ddim1  = 0;
         _tpReadOutBulk.transferDim.ddim2  = 0;
         _tpReadOutBulk.transferDim.ddim3  = 0;

         std::memset(&_tpReadOutTail, 0, sizeof(_tpReadOutTail)); // unused; sicnt0==0 skips it in exec()
      }

      // ---- Build each TR ONCE. prepareTr parses the TransferProp (flags, circular
      // props, icnt/dim) into a CSL_UdmapTR; none of that changes frame-to-frame.
      // exec() patches only tr->addr/tr->daddr (via runPreparedTr) and resubmits, so
      // the per-frame prepareTr cost is removed while staying in direct-TR mode.
      // ioPointers here are placeholders (_ring.pBuf is a valid non-null address);
      // they are overwritten every frame before the TR is triggered.
      if (errorCtx.isSuccess()) {
         _tpWriteInBulk.ioPointers.srcPtr = static_cast<uint8_t *>(_ring.pBuf);
         _tpWriteInBulk.ioPointers.dstPtr = static_cast<uint8_t *>(_ring.pBuf);
         _trPrep.trMem = _pTrMem.get();
         if (DmaUtilsAutoInc3d_prepareTr(&_trPrep, &_tpWriteInBulk) != UDMA_SOK) {
            errorCtx.setLibraryError("DelayDru constructor", "prepareTr (write-in) failed", 0);
         }

         _tpReadOutBulk.ioPointers.srcPtr = static_cast<uint8_t *>(_ring.pBuf);
         _tpReadOutBulk.ioPointers.dstPtr = static_cast<uint8_t *>(_ring.pBuf);
         _trPrep.trMem = _pTrMemRead.get();
         if (errorCtx.isSuccess() &&
             DmaUtilsAutoInc3d_prepareTr(&_trPrep, &_tpReadOutBulk) != UDMA_SOK) {
            errorCtx.setLibraryError("DelayDru constructor", "prepareTr (read-out) failed", 0);
         }
      }
#endif

      cleanInternalBuffers(); // zero the ring, reset _writeOff
   }
}

template <typename T> void *DelayDru<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *DelayDru<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t DelayDru<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t DelayDru<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void DelayDru<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _in.pBuf = addr.src[0];
   }
   if (mode == kAddrModeOut) {
      _out.pBuf = addr.dst[0];
   }
   if (mode == kAddrModeInOut) {
      _in.pBuf  = addr.src[0];
      _out.pBuf = addr.dst[0];
   }
}

#if !defined(HOST_EMULATION)
// ─────────────────────────────────────────────────────────────────────────────
// runPreparedTr — submit a TR that was already built once in the constructor.
// Only the source/destination addresses move frame-to-frame, so patch them in
// place — exactly what prepareTr's setupTr does (tr->addr = src, tr->daddr =
// dst) for a non-compressed transfer — then configure/trigger/wait/deconfigure.
// This keeps the low-latency direct-TR path (single-TR atomic submit) and lifts
// the structural prepareTr re-parse out of the per-frame hot loop.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T> void DelayDru<T>::runPreparedTr(uint8_t *trMem, const void *src, void *dst) noexcept
{
   CSL_UdmapTR *tr = reinterpret_cast<CSL_UdmapTR *>(trMem);
   tr->addr  = reinterpret_cast<uintptr_t>(src);
   tr->daddr = reinterpret_cast<uintptr_t>(dst);

   int32_t rv = DmaUtilsAutoInc3d_configure(_dmaUtilsContext.get(), _chId, trMem, 1);
   if (rv != UDMA_SOK) {
      (void) printf("[Error] DelayDru: configure failed (%d)\n", rv);
      return;
   }
   (void) DmaUtilsAutoInc3d_trigger(_dmaUtilsContext.get(), _chId);
   DmaUtilsAutoInc3d_wait(_dmaUtilsContext.get(), _chId);
   (void) DmaUtilsAutoInc3d_deconfigure(_dmaUtilsContext.get(), _chId, trMem, 1);
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// exec — one frame: write-in (circular DST) then one read-out per channel
// (circular SRC), then advance the ring write offset. Only the moving
// pointers (and the writeOff/readOff they depend on) are computed here — all
// descriptor structure was built once in the constructor.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T> void DelayDru<T>::exec() noexcept
{
#if !defined(HOST_EMULATION)
   // The DRU reads/writes _in/_out (L2SRAM) directly and the ring is non-cacheable DDR, so no
   // cache maintenance is done here. Callers own coherence of _in/_out with the CPU.
   uint8_t *pInBytes  = static_cast<uint8_t *>(_in.pBuf);
   uint8_t *pOutBytes = static_cast<uint8_t *>(_out.pBuf);

   // ---- Write-in first: single SYNC_2D TR, all channels. Must precede read-out so delays
   // shorter than a frame read the current frame's samples; ring >= frame + maxDelay
   // guarantees it never overwrites samples still to be read. ----
   const size_t writeByteOff = _writeOff * sizeof(T);
   uint8_t *pRingBytes = static_cast<uint8_t *>(_ring.pBuf);
   runPreparedTr(_pTrMem.get(), pInBytes, pRingBytes + writeByteOff);

   // ---- Read-out: one SYNC_1D TR per channel, only pointers move per frame. ----
   for (size_t c = 0; c < _numChannels; c++) {
      const size_t readOff     = (_writeOff + _ringSamplesPerCh - _delaySamples[c]) % _ringSamplesPerCh;
      const size_t readByteOff = readOff * sizeof(T);
      uint8_t     *ringBase    = pRingBytes + (_chRingSampOff[c] * sizeof(T)) + readByteOff;
      uint8_t     *outBase     = pOutBytes + (c * _frameBytes);

      runPreparedTr(_pTrMemRead.get(), ringBase, outBase);
   }

   _writeOff = (_writeOff + _frameSamples) % _ringSamplesPerCh;
#else
   // HOST_EMULATION: CPU ring model — same math as the PoC's App_delayReference,
   // running directly on the caller-allocated pRingDdr heap buffer. _chRingSampOff[c]
   // (precomputed in the ctor) gives each channel's ring base in samples.
   T *ring = static_cast<T *>(_ring.pBuf);
   for (size_t c = 0; c < _numChannels; c++) {
      T           *chRing  = ring + _chRingSampOff[c];
      const T     *chIn    = static_cast<const T *>(_in.pBuf) + (c * _frameSamples);
      T           *chOut   = static_cast<T *>(_out.pBuf) + (c * _frameSamples);
      const size_t readOff = (_writeOff + _ringSamplesPerCh - _delaySamples[c]) % _ringSamplesPerCh;

      for (size_t n = 0; n < _frameSamples; n++) {
         chRing[(_writeOff + n) % _ringSamplesPerCh] = chIn[n];
      }
      for (size_t n = 0; n < _frameSamples; n++) {
         chOut[n] = chRing[(readOff + n) % _ringSamplesPerCh];
      }
   }
   _writeOff = (_writeOff + _frameSamples) % _ringSamplesPerCh;
#endif
}

template <typename T> void DelayDru<T>::cleanInternalBuffers() noexcept
{
   if (_ring.pBuf != nullptr) {
      const size_t totalRingBytes = _numChannels * _ringBytesPerCh;
      std::memset(_ring.pBuf, 0, totalRingBytes);
#if !defined(HOST_EMULATION)
      // Ring is non-cacheable DDR (memset already lands directly in DDR); the
      // writeback-invalidate here is a safety guard in case the caller placed
      // pRingDdr in cacheable memory.
      CacheP_wbInv(_ring.pBuf, static_cast<int32_t>(totalRingBytes), CacheP_TYPE_ALL);
#endif
   }
   _writeOff = 0;
}

template <typename T> DelayDru<T>::~DelayDru()
{
#if !defined(HOST_EMULATION)
   if (_dmaUtilsInitDone) {
      (void) DmaUtilsAutoInc3d_deinit(_dmaUtilsContext.get());
   }
   // _dmaUtilsContext and _pTrMem (AlignedBuffer) release their memalign'd memory
   // automatically via AlignedDeleter — no explicit free() needed.
#endif
   // _delaySamples and _chRingSampOff (unique_ptr) release automatically.
}

// ─────────────────────────────────────────────────────────────────────────────
// Explicit instantiations
// ─────────────────────────────────────────────────────────────────────────────

template class DelayDru<float>;
template class DelayDru<double>;

} // namespace misc

} // namespace TISP
