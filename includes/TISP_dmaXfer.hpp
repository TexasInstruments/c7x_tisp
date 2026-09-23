#pragma once

#include "TISP_common.hpp"
#include "TISP_dmaRuntime.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

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
 *  \class DMAXfer
 *  @brief TISP node that transfers n memory buffers between L2 and DDR
 *         in a single graph step.
 *
 *  The template parameter W selects the DMA window mode:
 *    W=1 — sequential: configure → trigger → wait per channel (evict-before-load)
 *    W=2 — parallel:   configure all → trigger all → wait all (ping-pong)
 *
 *  The number of transfers n is a runtime constructor argument — no fixed
 *  upper bound.  Each of the n (src, dst, size) tuples is transferred when
 *  the node executes.  All source and destination pointers must be non-null.
 *
 *  On HOST_EMULATION the transfers use memcpy. On target, DmaUtilsAutoInc3d
 *  performs the actual DMA using one hardware channel per buffer.
 *
 *  Addresses are fixed at construction time; setAddr() is a no-op.
 *
 *  @tparam W  DMA window mode: 1 (sequential), 2 (parallel/ping-pong), 3 (future).
 *             Explicit instantiations are provided for W = 1, 2, 3.
 *
 *  @par Alignment
 *  For best C7x vector-load throughput, destination addresses in L2SRAM
 *  should be aligned to at least 64 bytes; 128 bytes preferred.
 */
template <int32_t W>
class DMAXfer final : public VirtualOp {

   int32_t  _n;          /*!< Number of transfers (runtime)                              */
   int32_t  _nChannels;  /*!< DRU channels allocated: 1 for W=1, _n for W=2             */
   bool     _initOk = true; /*!< False if UDMA or DmaUtils init failed; exec() is a no-op */
   std::unique_ptr<void*[]>   _srcAddrs; /*!< Source addresses  — array[_n]              */
   std::unique_ptr<void*[]>   _dstAddrs; /*!< Destination addrs — array[_n]              */
   std::unique_ptr<size_t[]>  _sizes;    /*!< Transfer sizes    — array[_n]              */

   // W=1: sequential — 1 channel reused, configure→trigger→wait per transfer.
   // W=2: two-phase  — ch0 evicts (phase 1), ch1+ch2 load in parallel (phase 2).

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

#if !defined(HOST_EMULATION)
   // ── DMA engine resources (target only) ───────────────────────────────────
   DMARuntime                    *_rt;              /*!< Borrowed runtime — nullptr means standalone   */
   AlignedBuffer                  _dmaUtilsContext; /*!< DmaUtils ctx — owned only when _rt==nullptr  */
   std::unique_ptr<uint8_t*[]>    _pTrMemCh;        /*!< Raw pointer view into _pTrMemBufs             */
   std::vector<AlignedBuffer>     _pTrMemBufs;      /*!< Aligned TR buffers per channel (owned)        */
   Udma_DrvObjectInt               _udmaDrvObj;     /*!< UDMA driver object — used when _rt==nullptr  */
   std::unique_ptr<DmaUtilsAutoInc3d_TransferDim[]> _transferDim; /*!< Transfer descriptors — array[2*_n] */
   std::unique_ptr<int32_t[]>     _chId;            /*!< DMA channel IDs       — array[_nChannels]    */
#endif

 public:

   const char          *getNodeName()     const noexcept override { return _nodeInfo.getNodeName();     }
   uint32_t             getNodeId()       const noexcept override { return _nodeInfo.getNodeId();       }
   const char          *getNodeParams()   const noexcept override { return _nodeInfo.getNodeParams();   }
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   void  *getInputAddr(size_t index)  const noexcept override;
   void  *getOutputAddr(size_t index) const noexcept override;
   size_t getNumInputs()              const noexcept override;
   size_t getNumOutputs()             const noexcept override;

   /**
    * @brief Construct a standalone DMAXfer<W> node that owns its UDMA driver.
    *        Use in unit tests and single-node scenarios.
    *
    * @param n         Number of (src, dst, size) transfer pairs.
    * @param srcAddrs  Array of n source addresses.  All elements must be non-null.
    * @param dstAddrs  Array of n destination addresses.  All elements must be non-null.
    * @param sizes     Array of n transfer sizes in bytes.
    */
   DMAXfer(int32_t n, void *const *srcAddrs, void *const *dstAddrs, size_t const *sizes,
           ErrorCtxt &errorCtx);

   /**
    * @brief Construct a DMAXfer<W> node that borrows a shared DMARuntime.
    *        Use in signal chains where multiple DMAXfer nodes share one UDMA
    *        driver instance (codegen path).
    *
    * @param rt         Shared DMARuntime — must outlive this node.
    * @param nChannels  Number of DRU channels this node uses (1 for W=1, 3 for W=2).
    * @param chIds      Array of at least nChannels DRU channel IDs (each in [0, 31])
    *                   assigned to this node.  The caller is responsible for ensuring
    *                   the array length matches nChannels; this cannot be verified from
    *                   a raw pointer.  Each element is range-checked at construction.
    * @param n          Number of (src, dst, size) transfer pairs.
    * @param srcAddrs   Array of n source addresses.
    * @param dstAddrs   Array of n destination addresses.
    * @param sizes      Array of n transfer sizes in bytes.
    */
   DMAXfer(DMARuntime &rt, int32_t nChannels, const int32_t *chIds,
           int32_t n, void *const *srcAddrs, void *const *dstAddrs, size_t const *sizes,
           ErrorCtxt &errorCtx);

   /**
    * @brief Execute all n transfers, skipping any with nullptr src or dst.
    *        W=1: sequential per-channel.  W>1: parallel (all configure, all trigger, wait).
    */
   void exec() noexcept override;

   /**
    * @brief No-op — DMA addresses are fixed at construction.
    */
   void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept override;

   ~DMAXfer();
};

// Explicit instantiation declarations — definitions are in TISP_dmaXfer.cpp.
// W encodes DMA window mode; n (runtime) controls the number of transfers.
extern template class DMAXfer<1>;
extern template class DMAXfer<2>;

/** @} */

} // namespace misc

} // namespace TISP
