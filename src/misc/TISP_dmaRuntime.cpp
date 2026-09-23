#include "TISP_dmaRuntime.hpp"

#include <cstdio>

#if !defined(HOST_EMULATION)
#include "../superNode/TISP_dmaUtils.hpp"
#include <cstdlib>
#endif

namespace TISP {

namespace misc {

DMARuntime::DMARuntime(uint32_t udmaInstId, int32_t numChannels, ErrorCtxt &errorCtx)
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
      errorCtx.setLibraryError("DMARuntime constructor", "Udma_init failed", static_cast<int32_t>(retVal));
      _initOk = false;
   }

   _ctx = AlignedBuffer(static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getContextSize(_numCh))));
   if (_ctx == nullptr) {
      errorCtx.setLibraryError("DMARuntime constructor", "memalign failed for DmaUtils context", 0);
      _initOk = false;
   } else {
      retVal = dmaUtils<uint8_t>::init(_ctx.get(), _numCh, hDrv);
      if (retVal != UDMA_SOK) {
         errorCtx.setLibraryError("DMARuntime constructor", "dmaUtils::init failed", static_cast<int32_t>(retVal));
         _initOk = false;
      }
   }
#else
   (void) udmaInstId;
   (void) numChannels;
#endif
}

DMARuntime::~DMARuntime()
{
#if !defined(HOST_EMULATION)
   if (_initOk) {
      (void) Udma_deinit(static_cast<Udma_DrvHandle>(&_drv));
   }
   // _ctx AlignedBuffer frees the aligned context buffer automatically.
#endif
}

} // namespace misc

} // namespace TISP
