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
// File-scope helper: decompose a flat byte transfer into a 2D DMA descriptor.
//
// icnt0 is uint16_t (max 65535).  For sizes > 65535, decompose into 2D:
//   icnt0 = largest power-of-two ≤ 32768 that evenly divides size
//   icnt1 = size / icnt0
// SYNC_2D fires one completion event after all icnt1 rows finish.
// ─────────────────────────────────────────────────────────────────────────────

#if !defined(HOST_EMULATION)
static void setupFlat(size_t size, DmaUtilsAutoInc3d_TransferDim *dim)
{
   uint16_t icnt0 = 1U;
   for (uint16_t p = 32768U; p >= 1U; p >>= 1U) {
      if (size % static_cast<size_t>(p) == 0U) {
         icnt0 = p;
         break;
      }
   }

   uint16_t icnt1 = static_cast<uint16_t>(size / static_cast<size_t>(icnt0));
   int32_t  dim1  = static_cast<int32_t>(icnt0);

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
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Standalone constructor — owns UDMA driver (used by unit tests and single-node
// scenarios).  Each instance runs its own Udma_init / Udma_deinit.
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W>
DMAXfer<W>::DMAXfer(int32_t n, void *const *srcAddrs, void *const *dstAddrs, size_t const *sizes)
    : _n(n), _nChannels((W == 1) ? 1 : n), _srcAddrs(new void *[n]), _dstAddrs(new void *[n]), _sizes(new size_t[n])
{
   for (int32_t i = 0; i < _n; i++) {
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
      (void) printf("[Error] DMAXfer<%d>: nChannels=%d exceeds DRU limit (16)\n", W, _nChannels);
   }

   _pTrMemCh    = new uint8_t *[_nChannels];
   _transferDim = new DmaUtilsAutoInc3d_TransferDim[_n]; // 1 descriptor per transfer
   _chId        = new int32_t[_nChannels];

   _dmaUtilsContext = static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getContextSize(_nChannels)));
   for (int32_t i = 0; i < _nChannels; i++) {
      _pTrMemCh[i] = static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getTrMemReq(1)));
      _chId[i]     = i;
   }

   // Initialise UDMA driver
   Udma_InitPrms  initPrms;
   Udma_DrvHandle drvHandle = static_cast<Udma_DrvHandle>(&_udmaDrvObj);
   UdmaInitPrms_init(UDMA_INST_ID_0, &initPrms);
   initPrms.instId    = UDMA_INST_ID_0;
   initPrms.enableUtc = UTRUE;
   uint32_t retVal    = Udma_init(drvHandle, &initPrms);
   if (UDMA_SOK != retVal) {
      (void) printf("[Error] DMAXfer<%d>: UDMA init failed\n", W);
   }

   // Initialise DmaUtils context for _nChannels channels
   retVal = dmaUtils<uint8_t>::init(_dmaUtilsContext, _nChannels, drvHandle);
   if (retVal != UDMA_SOK) {
      (void) printf("[Error] DMAXfer<%d>: DmaUtils init failed\n", W);
   }

   for (int32_t i = 0; i < _n; i++) {
      setupFlat(sizes[i], &_transferDim[i]);
   }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared-runtime constructor — borrows DMARuntime from the signal chain.
// No Udma_init / Udma_deinit; channel IDs are assigned by the caller (codegen).
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W>
DMAXfer<W>::DMAXfer(DMARuntime &rt, int32_t nChannels, const int32_t *chIds,
                    int32_t n, void *const *srcAddrs, void *const *dstAddrs, size_t const *sizes)
    : _n(n), _nChannels(nChannels), _srcAddrs(new void *[n]), _dstAddrs(new void *[n]), _sizes(new size_t[n])
{
   for (int32_t i = 0; i < _n; i++) {
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
   _rt              = &rt;      // borrow — runtime outlives all DMAXfer nodes in the chain
   _dmaUtilsContext = nullptr;  // not owned; access via _rt->context()

   if (_nChannels > 16) {
      (void) printf("[Error] DMAXfer<%d>: nChannels=%d exceeds DRU limit (16)\n", W, _nChannels);
   }

   _pTrMemCh    = new uint8_t *[_nChannels];
   _transferDim = new DmaUtilsAutoInc3d_TransferDim[_n];
   _chId        = new int32_t[_nChannels];

   for (int32_t i = 0; i < _nChannels; i++) {
      _pTrMemCh[i] = static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getTrMemReq(1)));
      _chId[i]     = chIds[i]; // caller-assigned — may reuse same ch across serialized nodes
   }

   for (int32_t i = 0; i < _n; i++) {
      setupFlat(sizes[i], &_transferDim[i]);
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
   for (int32_t i = 0; i < _nChannels; i++) {
      free(_pTrMemCh[i]);
   }
   if (_rt == nullptr) {
      // Standalone mode: we own both the context buffer and the UDMA driver.
      free(_dmaUtilsContext);
      (void) Udma_deinit(static_cast<Udma_DrvHandle>(&_udmaDrvObj));
   }
   // Shared mode: DMARuntime owns the context and driver — do not touch them here.
   delete[] _pTrMemCh;
   delete[] _transferDim;
   delete[] _chId;
#endif
   delete[] _srcAddrs;
   delete[] _dstAddrs;
   delete[] _sizes;
}

// ─────────────────────────────────────────────────────────────────────────────
// exec — execute all N transfers in order
// ─────────────────────────────────────────────────────────────────────────────

template <int32_t W> void DMAXfer<W>::exec() noexcept
{
#if !defined(HOST_EMULATION)
   // Resolve context: standalone owns _dmaUtilsContext; shared borrows from runtime.
   uint8_t *ctx = (_rt != nullptr) ? _rt->context() : _dmaUtilsContext;

   if (W == 2) {
      // Two-phase: 1-channel evict, then 2-channel parallel loads.
      // transfer[0] = state evict (L2→DDR) on ch0.
      // transfer[1] = coeff load (DDR→L2) on ch1.
      // transfer[2] = state load (DDR→L2) on ch2.
      // Phase-1 barrier prevents state-load from overwriting L2 before evict reads it.

      // Phase 1: evict on ch0 — configure, trigger, wait (1 channel)
      (void) dmaUtils<uint8_t>::configure_channel(
          ctx, _chId[0], _pTrMemCh, static_cast<uint8_t *>(_srcAddrs[0]),
          static_cast<uint8_t *>(_dstAddrs[0]), DMAUTILSAUTOINC3D_SYNC_2D, &_transferDim[0]);
      (void) dmaUtils<float>::trigger_single(ctx, &_chId[0], 0);
      dmaUtils<float>::wait_single(ctx, &_chId[0], 0);

      // Phase 2: coeff + state loads on ch1, ch2 — configure both, trigger both, wait both
      (void) dmaUtils<uint8_t>::configure_channel(
          ctx, _chId[1], _pTrMemCh, static_cast<uint8_t *>(_srcAddrs[1]),
          static_cast<uint8_t *>(_dstAddrs[1]), DMAUTILSAUTOINC3D_SYNC_2D, &_transferDim[1]);
      (void) dmaUtils<uint8_t>::configure_channel(
          ctx, _chId[2], _pTrMemCh, static_cast<uint8_t *>(_srcAddrs[2]),
          static_cast<uint8_t *>(_dstAddrs[2]), DMAUTILSAUTOINC3D_SYNC_2D, &_transferDim[2]);
      (void) dmaUtils<float>::trigger_single(ctx, &_chId[1], 0);
      (void) dmaUtils<float>::trigger_single(ctx, &_chId[2], 0);
      dmaUtils<float>::wait(ctx, &_chId[1], 2);
   } else {
      // W=1: single channel reused for all _n transfers (configure→trigger→wait per transfer).
      // _chId[0] is the one channel; _transferDim[i] supplies per-transfer src/dst/size.
      for (int32_t i = 0; i < _n; i++) {
         if (_srcAddrs[i] == nullptr || _dstAddrs[i] == nullptr) {
            continue;
         }
         (void) dmaUtils<uint8_t>::configure_channel(
             ctx, _chId[0], _pTrMemCh, static_cast<uint8_t *>(_srcAddrs[i]),
             static_cast<uint8_t *>(_dstAddrs[i]), DMAUTILSAUTOINC3D_SYNC_2D, &_transferDim[i]);
         (void) dmaUtils<float>::trigger_single(ctx, &_chId[0], 0);
         dmaUtils<float>::wait_single(ctx, &_chId[0], 0);
      }
   }
#else
   // Host emulation: sequential memcpy for all W — transfer ordering (evict before loads)
   // guarantees correctness without parallel simulation.
   for (int32_t i = 0; i < _n; i++) {
      if (_srcAddrs[i] == nullptr || _dstAddrs[i] == nullptr) {
         continue;
      }
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
// Explicit instantiations
// ─────────────────────────────────────────────────────────────────────────────

// W encodes DMA window mode — n (runtime) controls number of transfers.
template class DMAXfer<1>; // W=1: sequential — 1 channel, all transfers in order
template class DMAXfer<2>; // W=2: two-phase — 1-ch evict, then 2-ch parallel loads

} // namespace misc

} // namespace TISP
