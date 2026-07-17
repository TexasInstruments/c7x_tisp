// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_superNode_common.hpp"
#include <TISP_util.hpp>

namespace TISP {
namespace SuperNode {

#if defined(HOST_EMULATION)
Udma_DrvHandle BaseSuperNode::setUdmaDrvHandle()
#else
Udma_DrvHandle BaseSuperNode::setUdmaDrvHandle()
#endif
{
   Udma_InitPrms initPrms;

   uint32_t instId = 0;
   uint32_t retVal = 0;

   Udma_DrvHandle drvHandle = &_udmaDrvObj;

#if defined(HOST_EMULATION)
   instId = UDMA_INST_ID_MAIN_0;
#else
   instId = UDMA_INST_ID_0;
#endif
   UdmaInitPrms_init(instId, &initPrms);

   initPrms.instId = instId;

#if !defined(HOST_EMULATION)
   initPrms.enableUtc = UTRUE;
#endif
   retVal = Udma_init(drvHandle, &initPrms);

   if (UDMA_SOK != retVal) {
      (void) printf("Inside BaseSuperNode::setUdmaDrvHandle\n");
      (void) printf("[Error] UDMA init failed!!\n");
   }
   return drvHandle;
}

BaseSuperNode::BaseSuperNode(size_t numChannels, size_t numInChannels, size_t numOutChannels)
    : _numChannels(numChannels), _numInChannels(numInChannels), _numOutChannels(numOutChannels)

{
   // Initialize _nodeName with an empty string
   _dmaUtilsContext = static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getContextSize(_numChannels)));

   for (size_t ch = 0; ch < _numChannels; ch++) {
      _pTrMemCh[ch] = static_cast<uint8_t *>(memalign(128, DmaUtilsAutoInc3d_getTrMemReq(1)));
   }

   uint32_t retVal;

   setUdmaDrvHandle();

   // float is dummy as its not used by init function
   retVal = dmaUtils<float>::init(_dmaUtilsContext, _numChannels, static_cast<Udma_DrvHandle>(&_udmaDrvObj));

   if (retVal != UDMA_SOK) {
      (void) printf("[Error] UDMA init failed!!\n");
   }
}

void BaseSuperNode::execDefault(std::unique_ptr<opVec> &myOpVec)
{
   // Reset the node counters at the beginning of SuperNode execution
   NodeProfiler::resetCounters();

   uint32_t blockIdx     = 0;
   uint32_t firstTrigger = 0;

   // DMA trigger for pipe-up, out transfer is dummy and handled inside DMA
   // utility; trigger and wait are datatype independent so using float

   dmaUtils<float>::trigger(_dmaUtilsContext, _chIdIn, _numInChannels);

   // Wait for previous transfer of in
   dmaUtils<float>::wait(_dmaUtilsContext, _chIdIn, _numInChannels);

   while (1) {

      if (firstTrigger != 0) {

         blockIdx = dmaUtils<float>::trigger(_dmaUtilsContext, _chIdOut, _numOutChannels);
      }

      // DMA trigger for next in buffer
      if (blockIdx != 1) {
         dmaUtils<float>::trigger(_dmaUtilsContext, _chIdIn, _numInChannels);
      }

      execute::graph(myOpVec);

      if (blockIdx != 1) {
         dmaUtils<float>::wait(_dmaUtilsContext, _chIdIn, _numInChannels);
      }

      // Wait for previous transfer out
      if (firstTrigger != 0) {
         dmaUtils<float>::wait(_dmaUtilsContext, _chIdOut, _numOutChannels);
      }
      else {
         firstTrigger = 1;
      }

      if (blockIdx == 1) {
         break;
      }
   }

   dmaUtils<float>::trigger(_dmaUtilsContext, _chIdOut, _numOutChannels);

   // Need to wait for last out transfer
   dmaUtils<float>::wait(_dmaUtilsContext, _chIdOut, _numOutChannels);

   return;
}

} // namespace SuperNode
} // namespace TISP
