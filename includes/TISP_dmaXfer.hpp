#pragma once

#include "TISP_common.hpp"

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
 *  \class DMAXfer
 *  @brief TISP node that transfers n memory buffers between L2 and DDR
 *         in a single graph step.
 *
 *  The template parameter W selects the DMA window mode:
 *    W=1 — sequential: configure → trigger → wait per channel (evict-before-load)
 *    W=2 — parallel:   configure all → trigger all → wait all (ping-pong)
 *    W=3 — reserved for future W=3 window scheduling
 *
 *  The number of transfers n is a runtime constructor argument — no fixed
 *  upper bound.  Each of the n (src, dst, size) tuples is transferred when
 *  the node executes.  A transfer is skipped when either pointer is nullptr.
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
   void   **_srcAddrs;   /*!< Source addresses  — heap-allocated array[_n]               */
   void   **_dstAddrs;   /*!< Destination addrs — heap-allocated array[_n]               */
   size_t  *_sizes;      /*!< Transfer sizes    — heap-allocated array[_n]               */

   // W=1: sequential — 1 channel reused, configure→trigger→wait per transfer.
   // W=2: two-phase  — ch0 evicts (phase 1), ch1+ch2 load in parallel (phase 2).

   NodeInfo _nodeInfo; /*!< Node name, parameters, and ID */

#if !defined(HOST_EMULATION)
   // ── DMA engine resources (target only) ───────────────────────────────────
   uint8_t                       *_dmaUtilsContext; /*!< DmaUtils context buffer                      */
   uint8_t                      **_pTrMemCh;        /*!< TR memory per channel — array[_nChannels]    */
   Udma_DrvObjectInt               _udmaDrvObj;     /*!< UDMA driver object                           */
   DmaUtilsAutoInc3d_TransferDim  *_transferDim;    /*!< Transfer descriptors  — array[_n]            */
   int32_t                        *_chId;            /*!< DMA channel IDs       — array[_nChannels]    */
#endif

 public:

   const char          *getNodeName()     const noexcept override { return _nodeInfo.getNodeName();     }
   uint32_t             getNodeId()       const noexcept override { return _nodeInfo.getNodeId();       }
   const char          *getNodeParams()   const noexcept override { return _nodeInfo.getNodeParams();   }
   const NodeBufParams &getNodeBufParams() const noexcept override { return _nodeInfo.getNodeBufParams(); }

   /**
    * @brief Construct a DMAXfer<W> node.
    *
    * @param n         Number of (src, dst, size) transfer pairs.
    * @param srcAddrs  Array of n source addresses.  Pass nullptr for a slot to skip it.
    * @param dstAddrs  Array of n destination addresses.  Pass nullptr for a slot to skip it.
    * @param sizes     Array of n transfer sizes in bytes.
    */
   DMAXfer(int32_t n, void *const *srcAddrs, void *const *dstAddrs, size_t const *sizes);

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
extern template class DMAXfer<3>;

/** @} */

} // namespace misc

} // namespace TISP
