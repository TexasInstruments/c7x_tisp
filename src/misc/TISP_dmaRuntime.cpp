#include "TISP_dmaRuntime.hpp"

#include <cstdio>

#if !defined(HOST_EMULATION)
#include "../superNode/TISP_dmaUtils.hpp"
#include <cstdlib>
#endif

namespace TISP {

namespace misc {

DMARuntime::DMARuntime(uint32_t udmaInstId, int32_t numChannels)
{
#if !defined(HOST_EMULATION)
   _numCh = numChannels;

   Udma_InitPrms  initPrms;
   Udma_DrvHandle hDrv = static_cast<Udma_DrvHandle>(&_drv);
   UdmaInitPrms_init(udmaInstId, &initPrms);
   initPrms.instId    = udmaInstId;
   initPrms.enableUtc = UTRUE;

   uint32_t retVal = Udma_init(hDrv, &initPrms);
   if (retVal != UDMA_SOK) {
      (void) printf("[Error] DMARuntime: Udma_init failed (instId=%u)\n", udmaInstId);
   }

   _ctx    = static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getContextSize(_numCh)));
   retVal  = dmaUtils<uint8_t>::init(_ctx, _numCh, hDrv);
   if (retVal != UDMA_SOK) {
      (void) printf("[Error] DMARuntime: dmaUtils::init failed\n");
   }
#else
   (void) udmaInstId;
   (void) numChannels;
#endif
}

DMARuntime::~DMARuntime()
{
#if !defined(HOST_EMULATION)
   (void) Udma_deinit(static_cast<Udma_DrvHandle>(&_drv));
   free(_ctx);
#endif
}

} // namespace misc

} // namespace TISP
