#pragma once

#include "TISP_ErrorCtxt.hpp"

#include <cstdint>
#include <cstdlib>
#include <memory>

#if !defined(HOST_EMULATION)
#include "drivers/dmautils/src/dmautils_autoincrement_3d_priv.h"
#include <drivers/dmautils/dmautils.h>
#include <drivers/udma.h>
#endif

namespace TISP {

namespace misc {

struct AlignedDeleter { void operator()(void *p) const noexcept { free(p); } };
using AlignedBuffer = std::unique_ptr<uint8_t[], AlignedDeleter>;

/**
 *  @brief Owns one UDMA driver instance and a shared DmaUtils context for a
 *         signal chain.  Construct once per chain before any DMAXfer node is
 *         created; pass by reference to the DMAXfer<W>(rt, ...) constructor.
 *
 *  CLEC/DRU setup is NOT performed here — call App_dmautilsClecInitDru() once
 *  before constructing this object (codegen already emits that call).
 *
 *  @param udmaInstId   UDMA instance ID (typically UDMA_INST_ID_0).
 *  @param numChannels  Peak concurrent DRU channels needed across all DMAXfer
 *                      nodes in the chain.  W=1 → 1, any W=2 node → 3.
 */
class DMARuntime {
 public:
   DMARuntime(uint32_t udmaInstId, int32_t numChannels, ErrorCtxt &errorCtx);
   ~DMARuntime();

   DMARuntime(const DMARuntime &)            = delete;
   DMARuntime &operator=(const DMARuntime &) = delete;

   bool isReady() const noexcept { return _initOk; }

#if !defined(HOST_EMULATION)
   uint8_t       *context()   const noexcept { return _ctx.get(); }
   Udma_DrvHandle drvHandle() noexcept { return static_cast<Udma_DrvHandle>(&_drv); }
#endif

 private:
   bool _initOk = true; /*!< False if Udma_init or dmaUtils::init failed */

#if !defined(HOST_EMULATION)
   Udma_DrvObjectInt _drv;    /*!< UDMA driver object — owned by this runtime */
   AlignedBuffer     _ctx;    /*!< DmaUtils context buffer — aligned, owned   */
   int32_t           _numCh;  /*!< Channel count this context was sized for    */
#endif
};

} // namespace misc

} // namespace TISP
