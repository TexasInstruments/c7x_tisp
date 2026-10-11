#include "../common/TISP_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_dmaXfer.hpp>

#include <cstdio>
#include <cstring>

#if !defined(HOST_EMULATION)
#include "../superNode/TISP_dmaUtils.hpp"
#endif

namespace TISP {

namespace misc {

// ─────────────────────────────────────────────────────────────────────────────
// File-scope helpers: decompose a flat byte transfer into DMA descriptors.
//
// A single contiguous AutoInc 2D descriptor transfers exactly icnt0*icnt1 bytes,
// so the innermost burst icnt0 must EVENLY DIVIDE size.  The old scheme picked
// icnt0 = largest power-of-two dividing size, which collapses to an 8/16/32-byte
// burst whenever size carries a large odd factor (the common case — partition
// sizes are channels × frame × taps, products of odd primes).  A tiny burst
// streams the whole buffer in dribbles, paying per-row DRU overhead on tens of
// thousands of rows — the dominant DMA cost in the L2-partitioned chains.
//
// planFlat instead uses a FIXED large burst and mops up the remainder with a
// second small descriptor:
//   bulk : icnt0 = B (largest power-of-two ≤ min(size, 32768)), icnt1 = size / B
//   tail : the (size % B) leftover, tiled by the old largest-pow2-divisor rule
// Both descriptors are fully contiguous (dim1 = icnt0).  The tail is skipped
// when size is a multiple of B (tail counts left at 0).  This keeps the burst
// large regardless of divisibility and removes the icnt1 > 65535 overflow the
// old scheme hit on the largest partitions.
// ─────────────────────────────────────────────────────────────────────────────

#if !defined(HOST_EMULATION)
static const size_t  DMAXFER_MAX_BURST  = 32768U; // AutoInc icnt0 ceiling
static const int32_t DMAXFER_MAX_DRU_CH = 32;     // DRU channel count across all supported SoCs

// Largest power-of-two ≤ min(size, DMAXFER_MAX_BURST); ≥ 1 for size ≥ 1.
static uint16_t pickBurst(size_t size)
{
   uint16_t b = 1U;
   for (uint16_t p = static_cast<uint16_t>(DMAXFER_MAX_BURST); p >= 1U; p >>= 1U) {
      if (static_cast<size_t>(p) <= size) {
         b = p;
         break;
      }
   }
   return b;
}

// Fill one fully-contiguous 2D descriptor covering exactly icnt0*icnt1 bytes.
static void fill2D(DmaUtilsAutoInc3d_TransferDim *dim, uint16_t icnt0, uint16_t icnt1)
{
   int32_t dim1 = static_cast<int32_t>(icnt0);

   dim->sicnt0 = icnt0;
   dim->dicnt0 = icnt0;
   dim->sicnt1 = icnt1;
   dim->dicnt1 = icnt1;
   dim->sicnt2 = 1U;
   dim->dicnt2 = 1U;
   dim->sicnt3 = 1U;
   dim->dicnt3 = 1U;
   dim->sdim1  = dim1;
   dim->ddim1  = dim1;
   dim->sdim2  = 0;
   dim->ddim2  = 0;
   dim->sdim3  = 0;
   dim->ddim3  = 0;
}

// Plan a flat transfer of `size` bytes as bulk (fixed burst) + optional tail.
// bulk and tail are adjacent descriptor slots; tail is zeroed (skipped) when
// size is an exact multiple of the chosen burst.  The bulk covers
// (bulk.icnt0 * bulk.icnt1) bytes, so the tail's byte offset is derivable from
// the bulk dims — no extra bookkeeping needed by exec().
static void planFlat(size_t size,
                     DmaUtilsAutoInc3d_TransferDim *bulk,
                     DmaUtilsAutoInc3d_TransferDim *tail)
{
   uint16_t B = pickBurst(size);            // large power-of-two burst, ≤ size
   uint16_t q = static_cast<uint16_t>(size / static_cast<size_t>(B)); // full-burst rows

   // q fits uint16_t for any L2-sized transfer: B = 32768 for size > 32768, so
   // q ≤ size/32768 (≈ 32 for the largest ~1 MB partitions).  Flag if exceeded.
   if (size / static_cast<size_t>(B) > 65535U) {
      (void) printf("[Error] DMAXfer planFlat: bulk rows %zu exceed uint16_t\n",
                    size / static_cast<size_t>(B));
   }

   fill2D(bulk, B, q);

   size_t bulkBytes = static_cast<size_t>(B) * static_cast<size_t>(q);
   size_t r         = size - bulkBytes;     // remainder < B
   if (r == 0U) {
      // Exact multiple — no tail.  Zero counts so exec() skips it.
      fill2D(tail, 0U, 0U);
      return;
   }

   // Tail: ONE row of exactly r bytes.  DmaUtilsAutoInc3d hardcodes ELYPE_1
   // (1-byte element type) for all linear transfers, so icnt0 is a plain byte
   // count with no power-of-two or bus-width alignment requirement.  r < B ≤
   // DMAXFER_MAX_BURST ≤ 65535, so one row covers the remainder exactly.
   fill2D(tail, static_cast<uint16_t>(r), 1U);
}

// Run one logical flat transfer (bulk + optional tail) on a single channel,
// serialized: configure → trigger → wait per sub-descriptor.  The tail starts
// at src/dst + (bulk.icnt0 * bulk.icnt1) bytes.
static void runFlatSerial(uint8_t *ctx, int32_t chId, uint8_t **pTrMem,
                          uint8_t *src, uint8_t *dst,
                          DmaUtilsAutoInc3d_TransferDim *bulk,
                          DmaUtilsAutoInc3d_TransferDim *tail)
{
   (void) dmaUtils<uint8_t>::configure_channel(
       ctx, chId, pTrMem, src, dst, DMAUTILSAUTOINC3D_SYNC_2D, bulk);
   (void) dmaUtils<float>::trigger_single(ctx, &chId, 0);
   dmaUtils<float>::wait_single(ctx, &chId, 0);

   if (tail->sicnt0 != 0U && tail->sicnt1 != 0U) {
      size_t off = static_cast<size_t>(bulk->sicnt0) * static_cast<size_t>(bulk->sicnt1);
      (void) dmaUtils<uint8_t>::configure_channel(
          ctx, chId, pTrMem, src + off, dst + off, DMAUTILSAUTOINC3D_SYNC_2D, tail);
      (void) dmaUtils<float>::trigger_single(ctx, &chId, 0);
      dmaUtils<float>::wait_single(ctx, &chId, 0);
   }
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Standalone constructor — owns UDMA driver (used by unit tests and single-node
// scenarios).  Each instance runs its own Udma_init / Udma_deinit.
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W>
DMAXfer<W>::DMAXfer(int32_t n, void *const *srcAddrs, void *const *dstAddrs, size_t const *sizes,
                    ErrorCtxt &errorCtx)
    : _n(n), _nChannels((W == 1) ? 1 : n),
      _srcAddrs(std::make_unique<void *[]>(n)),
      _dstAddrs(std::make_unique<void *[]>(n)),
      _sizes(std::make_unique<size_t[]>(n))
{
   if (n <= 0)              { errorCtx.setInvalidParameterError("DMAXfer constructor", "n must be > 0");    _initOk = false; }
   if (srcAddrs == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "srcAddrs is null");      _initOk = false; }
   if (dstAddrs == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "dstAddrs is null");      _initOk = false; }
   if (sizes    == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "sizes is null");         _initOk = false; }

   for (int32_t i = 0; i < _n; i++) {
      if (srcAddrs[i] == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "srcAddrs element is null"); _initOk = false; }
      if (dstAddrs[i] == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "dstAddrs element is null"); _initOk = false; }
      _srcAddrs[i] = srcAddrs[i];
      _dstAddrs[i] = dstAddrs[i];
      _sizes[i]    = sizes[i];
   }

   // Build params string: "w=W,n=n,ch=C,size0=X,..."
   {
      auto  &buf = _nodeInfo.params;
      size_t cap = buf.size();
      size_t pos = 0U;
      pos += static_cast<size_t>(snprintf(buf.data() + pos, cap - pos, "w=%d,n=%d,ch=%d", W, _n, _nChannels));
      for (int32_t i = 0; i < _n && pos < cap; i++) {
         pos += static_cast<size_t>(snprintf(buf.data() + pos, cap - pos, ",size%d=%zu", i, sizes[i]));
      }
   }
   _nodeInfo.setName("DMAXfer");

#if !defined(HOST_EMULATION)
   _rt = nullptr; // standalone — this instance owns the UDMA driver

   // W=1: 1 channel reused for all _n transfers (sequential, no DRU channel waste).
   // W>1: _n channels for parallel execution.
   // CLEC initialises 16 DRU channels; validate channel count at runtime.
   if (_nChannels > 16) {
      errorCtx.setInvalidParameterError("DMAXfer constructor", "nChannels exceeds DRU limit (16)");
      _initOk = false;
   }
   if (W == 2) {
      if (_n != 3) {
         errorCtx.setInvalidParameterError("DMAXfer<2> constructor", "requires exactly 3 transfers (evict+coeff+state)");
         _initOk = false;
      }
   }

   if (errorCtx.isSuccess()) {
      _pTrMemCh    = std::make_unique<uint8_t *[]>(_nChannels);
      _transferDim = std::make_unique<DmaUtilsAutoInc3d_TransferDim[]>(2 * _n);
      _chId        = std::make_unique<int32_t[]>(_nChannels);

      _dmaUtilsContext = AlignedBuffer(static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getContextSize(_nChannels))));
      if (_dmaUtilsContext == nullptr) {
         errorCtx.setLibraryError("DMAXfer constructor", "memalign failed for DmaUtils context", 0);
         _initOk = false;
      }
      _pTrMemBufs.resize(_nChannels);
      for (int32_t i = 0; i < _nChannels; i++) {
         _pTrMemBufs[i] = AlignedBuffer(static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getTrMemReq(1))));
         if (_pTrMemBufs[i] == nullptr) {
            errorCtx.setLibraryError("DMAXfer constructor", "memalign failed for TR memory", 0);
            _initOk = false;
         }
         _pTrMemCh[i]   = _pTrMemBufs[i].get();
         _chId[i]       = i;
      }

      // Initialise UDMA driver
      Udma_InitPrms  initPrms;
      Udma_DrvHandle drvHandle = static_cast<Udma_DrvHandle>(&_udmaDrvObj);
      UdmaInitPrms_init(UDMA_INST_ID_0, &initPrms);
      initPrms.instId    = UDMA_INST_ID_0;
      initPrms.enableUtc = UTRUE;
      uint32_t retVal    = Udma_init(drvHandle, &initPrms);
      if (UDMA_SOK != retVal) {
         errorCtx.setLibraryError("DMAXfer constructor", "Udma_init failed", static_cast<int32_t>(retVal));
         _initOk = false;
      }

      // Initialise DmaUtils context for _nChannels channels
      retVal = dmaUtils<uint8_t>::init(_dmaUtilsContext.get(), _nChannels, drvHandle);
      if (retVal != UDMA_SOK) {
         errorCtx.setLibraryError("DMAXfer constructor", "dmaUtils::init failed", static_cast<int32_t>(retVal));
         _initOk = false;
      }

      for (int32_t i = 0; i < _n; i++) {
         planFlat(sizes[i], &_transferDim[2 * i], &_transferDim[2 * i + 1]);
      }
   }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared-runtime constructor — borrows DMARuntime from the signal chain.
// No Udma_init / Udma_deinit; channel IDs are assigned by the caller (codegen).
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W>
DMAXfer<W>::DMAXfer(DMARuntime &rt, int32_t nChannels, const int32_t *chIds,
                    int32_t n, void *const *srcAddrs, void *const *dstAddrs, size_t const *sizes,
                    ErrorCtxt &errorCtx)
    : _n(n), _nChannels(nChannels),
      _srcAddrs(std::make_unique<void *[]>(n)),
      _dstAddrs(std::make_unique<void *[]>(n)),
      _sizes(std::make_unique<size_t[]>(n))
{
   if (n <= 0)              { errorCtx.setInvalidParameterError("DMAXfer constructor", "n must be > 0");        _initOk = false; }
   if (nChannels <= 0)      { errorCtx.setInvalidParameterError("DMAXfer constructor", "nChannels must be > 0"); _initOk = false; }
   if (chIds    == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "chIds is null");             _initOk = false; }
   if (srcAddrs == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "srcAddrs is null");          _initOk = false; }
   if (dstAddrs == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "dstAddrs is null");          _initOk = false; }
   if (sizes    == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "sizes is null");             _initOk = false; }

   for (int32_t i = 0; i < _n; i++) {
      if (srcAddrs[i] == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "srcAddrs element is null"); _initOk = false; }
      if (dstAddrs[i] == nullptr) { errorCtx.setNullPointerError("DMAXfer constructor", "dstAddrs element is null"); _initOk = false; }
      _srcAddrs[i] = srcAddrs[i];
      _dstAddrs[i] = dstAddrs[i];
      _sizes[i]    = sizes[i];
   }

   {
      auto  &buf = _nodeInfo.params;
      size_t cap = buf.size();
      size_t pos = 0U;
      pos += static_cast<size_t>(snprintf(buf.data() + pos, cap - pos, "w=%d,n=%d,ch=%d", W, _n, _nChannels));
      for (int32_t i = 0; i < _n && pos < cap; i++) {
         pos += static_cast<size_t>(snprintf(buf.data() + pos, cap - pos, ",size%d=%zu", i, sizes[i]));
      }
   }
   _nodeInfo.setName("DMAXfer");

#if !defined(HOST_EMULATION)
   _rt = &rt;  // borrow — runtime outlives all DMAXfer nodes in the chain
               // _dmaUtilsContext left as nullptr (not owned; access via _rt->context())

   if (!rt.isReady()) {
      errorCtx.setLibraryError("DMAXfer constructor", "DMARuntime init failed", 0);
      _initOk = false;
   }

   if (_nChannels > 16) {
      errorCtx.setInvalidParameterError("DMAXfer constructor", "nChannels exceeds DRU limit (16)");
      _initOk = false;
   }
   if (W == 2) {
      if (_n != 3 || _nChannels != 3) {
         errorCtx.setInvalidParameterError("DMAXfer<2> constructor", "requires exactly 3 transfers and 3 channels (evict+coeff+state)");
         _initOk = false;
      }
   }

   if (errorCtx.isSuccess()) {
      _pTrMemCh    = std::make_unique<uint8_t *[]>(_nChannels);
      _transferDim = std::make_unique<DmaUtilsAutoInc3d_TransferDim[]>(2 * _n);
      _chId        = std::make_unique<int32_t[]>(_nChannels);

      _pTrMemBufs.resize(_nChannels);
      for (int32_t i = 0; i < _nChannels; i++) {
         if (chIds[i] < 0 || chIds[i] >= DMAXFER_MAX_DRU_CH) {
            errorCtx.setInvalidParameterError("DMAXfer constructor", "chIds element out of range [0, 31]");
            _initOk = false;
         }
         _pTrMemBufs[i] = AlignedBuffer(static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getTrMemReq(1))));
         if (_pTrMemBufs[i] == nullptr) {
            errorCtx.setLibraryError("DMAXfer constructor", "memalign failed for TR memory", 0);
            _initOk = false;
         }
         _pTrMemCh[i]   = _pTrMemBufs[i].get();
         _chId[i]       = chIds[i];
      }
   }

   for (int32_t i = 0; i < _n; i++) {
      planFlat(sizes[i], &_transferDim[2 * i], &_transferDim[2 * i + 1]);
   }
   // No Udma_init — DMARuntime already initialised the driver
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Destructor
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W> DMAXfer<W>::~DMAXfer()
{
#if !defined(HOST_EMULATION)
   if (_rt == nullptr) {
      // Standalone mode: UDMA driver must be explicitly de-initialised before
      // the context buffer (_dmaUtilsContext) is freed by its AlignedBuffer dtor.
      (void) Udma_deinit(static_cast<Udma_DrvHandle>(&_udmaDrvObj));
   }
   // All smart-pointer members (_dmaUtilsContext, _pTrMemBufs, _pTrMemCh,
   // _transferDim, _chId) release their resources automatically.
#endif
   // _srcAddrs, _dstAddrs, _sizes released automatically.
}

// ─────────────────────────────────────────────────────────────────────────────
// exec — execute all N transfers in order
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W> void DMAXfer<W>::exec() noexcept
{
#if !defined(HOST_EMULATION)
   if (!_initOk) { return; }

   // Resolve context: standalone owns _dmaUtilsContext; shared borrows from runtime.
   uint8_t *ctx = (_rt != nullptr) ? _rt->context() : _dmaUtilsContext.get();

   // Each transfer i occupies two descriptor slots: bulk = _transferDim[2i],
   // tail = _transferDim[2i+1] (tail counts are 0 when size is burst-aligned).
   if (W == 2) {
      // Two-phase: 1-channel evict, then 2-channel parallel loads.
      // transfer[0] = state evict (L2→DDR) on ch0.
      // transfer[1] = coeff load (DDR→L2) on ch1.
      // transfer[2] = state load (DDR→L2) on ch2.
      // Phase-1 barrier prevents state-load from overwriting L2 before evict reads it.

      // Phase 1: evict on ch0 — bulk + optional tail, serialized.
      runFlatSerial(ctx, _chId[0], _pTrMemCh.get(), static_cast<uint8_t *>(_srcAddrs[0]),
                    static_cast<uint8_t *>(_dstAddrs[0]), &_transferDim[0], &_transferDim[1]);

      // Phase 2: coeff (ch1) + state (ch2) loads.  Bulks run in parallel; any
      // tails run in parallel after, so the dominant bulk keeps full overlap.
      DmaUtilsAutoInc3d_TransferDim *b1 = &_transferDim[2], *t1 = &_transferDim[3];
      DmaUtilsAutoInc3d_TransferDim *b2 = &_transferDim[4], *t2 = &_transferDim[5];
      uint8_t *s1 = static_cast<uint8_t *>(_srcAddrs[1]), *d1 = static_cast<uint8_t *>(_dstAddrs[1]);
      uint8_t *s2 = static_cast<uint8_t *>(_srcAddrs[2]), *d2 = static_cast<uint8_t *>(_dstAddrs[2]);

      (void) dmaUtils<uint8_t>::configure_channel(ctx, _chId[1], _pTrMemCh.get(), s1, d1, DMAUTILSAUTOINC3D_SYNC_2D, b1);
      (void) dmaUtils<uint8_t>::configure_channel(ctx, _chId[2], _pTrMemCh.get(), s2, d2, DMAUTILSAUTOINC3D_SYNC_2D, b2);
      (void) dmaUtils<float>::trigger_single(ctx, &_chId[1], 0);
      (void) dmaUtils<float>::trigger_single(ctx, &_chId[2], 0);
      dmaUtils<float>::wait(ctx, &_chId[1], 2);

      bool tail1 = (t1->sicnt0 != 0U && t1->sicnt1 != 0U);
      bool tail2 = (t2->sicnt0 != 0U && t2->sicnt1 != 0U);
      if (tail1) {
         size_t off1 = static_cast<size_t>(b1->sicnt0) * static_cast<size_t>(b1->sicnt1);
         (void) dmaUtils<uint8_t>::configure_channel(ctx, _chId[1], _pTrMemCh.get(), s1 + off1, d1 + off1, DMAUTILSAUTOINC3D_SYNC_2D, t1);
         (void) dmaUtils<float>::trigger_single(ctx, &_chId[1], 0);
      }
      if (tail2) {
         size_t off2 = static_cast<size_t>(b2->sicnt0) * static_cast<size_t>(b2->sicnt1);
         (void) dmaUtils<uint8_t>::configure_channel(ctx, _chId[2], _pTrMemCh.get(), s2 + off2, d2 + off2, DMAUTILSAUTOINC3D_SYNC_2D, t2);
         (void) dmaUtils<float>::trigger_single(ctx, &_chId[2], 0);
      }
      if (tail1 && tail2)  dmaUtils<float>::wait(ctx, &_chId[1], 2);
      else if (tail1)      dmaUtils<float>::wait_single(ctx, &_chId[1], 0);
      else if (tail2)      dmaUtils<float>::wait_single(ctx, &_chId[2], 0);
   } else {
      // W=1: single channel reused for all _n transfers, each bulk + optional tail.
      for (int32_t i = 0; i < _n; i++) {
         runFlatSerial(ctx, _chId[0], _pTrMemCh.get(), static_cast<uint8_t *>(_srcAddrs[i]),
                       static_cast<uint8_t *>(_dstAddrs[i]), &_transferDim[2 * i], &_transferDim[2 * i + 1]);
      }
   }
#else
   // Host emulation: sequential memcpy for all W — transfer ordering (evict before loads)
   // guarantees correctness without parallel simulation.
   for (int32_t i = 0; i < _n; i++) {
      (void) memcpy(_dstAddrs[i], _srcAddrs[i], _sizes[i]);
   }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// setAddr — no-op
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W> void DMAXfer<W>::setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept
{
   (void) addrs;
   (void) mode;
}

// ─────────────────────────────────────────────────────────────────────────────
// Buffer topology accessors — expose src/dst layout to the graph framework
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W> void *DMAXfer<W>::getInputAddr(size_t index) const noexcept
{
   return (index < static_cast<size_t>(_n)) ? _srcAddrs[index] : nullptr;
}

template <int32_t W> void *DMAXfer<W>::getOutputAddr(size_t index) const noexcept
{
   return (index < static_cast<size_t>(_n)) ? _dstAddrs[index] : nullptr;
}

template <int32_t W> size_t DMAXfer<W>::getNumInputs() const noexcept
{
   return static_cast<size_t>(_n);
}

template <int32_t W> size_t DMAXfer<W>::getNumOutputs() const noexcept
{
   return static_cast<size_t>(_n);
}

// ─────────────────────────────────────────────────────────────────────────────
// Explicit instantiations
// ─────────────────────────────────────────────────────────────────────────────

// W encodes DMA window mode — n (runtime) controls number of transfers.
template class DMAXfer<1>; // W=1: sequential — 1 channel, all transfers in order
template class DMAXfer<2>; // W=2: two-phase — 1-ch evict, then 2-ch parallel loads

} // namespace misc

} // namespace TISP
