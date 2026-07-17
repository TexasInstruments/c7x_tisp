// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_superNode_common.hpp"
#include <cmath>

namespace TISP {

template <typename dType>
void dmaUtils<dType>::dmaAutoIncSetupXferPropIn2D(size_t                         width,
                                                  size_t                         height,
                                                  size_t                         blockWidth,
                                                  size_t                         blockHeight,
                                                  size_t                         inPitch,
                                                  size_t                         outPitch,
                                                  DmaUtilsAutoInc3d_TransferDim *transferDim)
{
   (void) outPitch;

   uint32_t pingPongOffsetIn = static_cast<uint32_t>(blockWidth * blockHeight * sizeof(dType));

   transferDim->sicnt0 = static_cast<uint16_t>(blockWidth * sizeof(dType));
   transferDim->sicnt1 = static_cast<uint16_t>(blockHeight);
   transferDim->sicnt2 = static_cast<uint16_t>(width / blockWidth);
   transferDim->sicnt3 = static_cast<uint16_t>(height / blockHeight);
   transferDim->sdim1  = static_cast<int32_t>(inPitch);
   transferDim->sdim2  = static_cast<int32_t>(blockWidth * sizeof(dType));
   transferDim->sdim3  = static_cast<int32_t>(blockHeight * inPitch);

   transferDim->dicnt0 = static_cast<uint16_t>(blockWidth * sizeof(dType));
   transferDim->dicnt1 = static_cast<uint16_t>(blockHeight);
   transferDim->dicnt2 = static_cast<uint16_t>(2U);
   transferDim->dicnt3 = static_cast<uint16_t>(
       (static_cast<uint32_t>(transferDim->sicnt2) * static_cast<uint32_t>(transferDim->sicnt3) + 1U) / 2U);
   transferDim->ddim1 = static_cast<int32_t>(blockWidth * sizeof(dType));
   transferDim->ddim2 = static_cast<int32_t>(pingPongOffsetIn);
   transferDim->ddim3 = 0;

   return;
}

template <typename dType>
void dmaUtils<dType>::dmaAutoIncSetupXferPropOut2D(size_t                         width,
                                                   size_t                         height,
                                                   size_t                         blockWidth,
                                                   size_t                         blockHeight,
                                                   size_t                         inPitch,
                                                   size_t                         outPitch,
                                                   DmaUtilsAutoInc3d_TransferDim *transferDim)
{
   (void) inPitch;

   uint32_t pingPongOffset = static_cast<uint32_t>(blockWidth * blockHeight * sizeof(dType));

   transferDim->sicnt0 = static_cast<uint16_t>(blockWidth * sizeof(dType));
   transferDim->sicnt1 = static_cast<uint16_t>(blockHeight);
   transferDim->sicnt2 = static_cast<uint16_t>(2U);
   transferDim->sicnt3 = static_cast<uint16_t>(((width / blockWidth) * (height / blockHeight) + 1U) / 2U);
   transferDim->sdim1  = static_cast<int32_t>(blockWidth * sizeof(dType));
   transferDim->sdim2  = static_cast<int32_t>(pingPongOffset);
   transferDim->sdim3  = 0;

   transferDim->dicnt0 = static_cast<uint16_t>(blockWidth * sizeof(dType));
   transferDim->dicnt1 = static_cast<uint16_t>(blockHeight);
   transferDim->dicnt2 = static_cast<uint16_t>(width / blockWidth);
   transferDim->dicnt3 = static_cast<uint16_t>(height / blockHeight);
   transferDim->ddim1  = static_cast<int32_t>(outPitch);
   transferDim->ddim2  = static_cast<int32_t>(blockWidth * sizeof(dType));
   transferDim->ddim3  = static_cast<int32_t>(outPitch * blockHeight);

   return;
}

template <typename dType>
void dmaUtils<dType>::dmaAutoIncSetupXferPropOut2DTranspose(size_t                         width,
                                                            size_t                         height,
                                                            size_t                         blockWidth,
                                                            size_t                         blockHeight,
                                                            size_t                         inPitch,
                                                            size_t                         outPitch,
                                                            DmaUtilsAutoInc3d_TransferDim *transferDim)
{
   (void) inPitch;

   uint32_t pingPongOffset = static_cast<uint32_t>(blockWidth * blockHeight * sizeof(dType));

   transferDim->sicnt0 = static_cast<uint16_t>(blockHeight * sizeof(dType));
   transferDim->sicnt1 = static_cast<uint16_t>(blockWidth);
   transferDim->sicnt2 = static_cast<uint16_t>(2U);
   transferDim->sicnt3 = static_cast<uint16_t>(((width / blockWidth) * (height / blockHeight) + 1U) / 2U);
   transferDim->sdim1  = static_cast<int32_t>(blockHeight * sizeof(dType));
   transferDim->sdim2  = static_cast<int32_t>(pingPongOffset);
   transferDim->sdim3  = 0;

   transferDim->dicnt0 = static_cast<uint16_t>(blockHeight * sizeof(dType));
   transferDim->dicnt1 = static_cast<uint16_t>(blockWidth);
   transferDim->dicnt2 = static_cast<uint16_t>(width / blockWidth);
   transferDim->dicnt3 = static_cast<uint16_t>(height / blockHeight);
   transferDim->ddim1  = static_cast<int32_t>(outPitch);
   transferDim->ddim2  = static_cast<int32_t>(outPitch * blockWidth);
   transferDim->ddim3  = static_cast<int32_t>(blockHeight * sizeof(dType));

   return;
}

template <typename dType>
uint32_t dmaUtils<dType>::init(uint8_t *dmaUtilsContext, int32_t numChannels, Udma_DrvHandle drvHandle)
{

   int32_t                     retVal = UDMA_SOK;
   DmaUtilsAutoInc3d_InitParam initParams;

   DmaUtilsAutoInc3d_ChannelInitParam chInitParams[numChannels]; // NOLINT(MISRA.ARRAY.CSTYLE)

   initParams.contextSize     = DmaUtilsAutoInc3d_getContextSize(numChannels);
   initParams.numChannels     = numChannels;
   initParams.traceLogLevel   = 1;
   initParams.udmaDrvHandle   = drvHandle;
   initParams.DmaUtilsVprintf = vprintf;

   for (int32_t ch = 0; ch < numChannels; ch++) {
      chInitParams[ch].dmaQueNo = 0;
      chInitParams[ch].druOwner = DMAUTILSAUTOINC3D_DRUOWNER_DIRECT_TR;
   }

   retVal = DmaUtilsAutoInc3d_init(dmaUtilsContext, &initParams, chInitParams);

   return static_cast<uint32_t>(retVal);
}

template <typename dType>
uint32_t dmaUtils<dType>::configure_channel(uint8_t                       *dmaUtilsContext,
                                            int32_t                        ch,
                                            uint8_t                      **pTrMemChs,
                                            uint8_t                       *srcPtr,
                                            uint8_t                       *dstPtr,
                                            uint32_t                       syncType,
                                            DmaUtilsAutoInc3d_TransferDim *transferDim)
{
   uint32_t                         retVal = UDMA_SOK;
   DmaUtilsAutoInc3d_TrPrepareParam trPrepParam;
   DmaUtilsAutoInc3d_TransferProp   transferProp;

   trPrepParam.channelId = ch;
   trPrepParam.numTRs    = 1;
   trPrepParam.trMem     = pTrMemChs[ch];
   trPrepParam.trMemSize = DmaUtilsAutoInc3d_getTrMemReq(1);

   transferProp.circProp.circDir       = DMAUTILSAUTOINC3D_CIRCDIR_SRC;
   transferProp.circProp.circSize1     = 0;
   transferProp.circProp.circSize2     = 0;
   transferProp.circProp.addrModeIcnt0 = DMAUTILSAUTOINC3D_ADDR_LINEAR;
   transferProp.circProp.addrModeIcnt1 = DMAUTILSAUTOINC3D_ADDR_LINEAR;
   transferProp.circProp.addrModeIcnt2 = DMAUTILSAUTOINC3D_ADDR_LINEAR;
   transferProp.circProp.addrModeIcnt3 = DMAUTILSAUTOINC3D_ADDR_LINEAR;

   transferProp.syncType          = syncType;
   transferProp.dmaDfmt           = DMAUTILSAUTOINC3D_DFMT_NONE;
   transferProp.ioPointers.srcPtr = srcPtr;
   transferProp.ioPointers.dstPtr = dstPtr;
   transferProp.transferDim       = *transferDim;

   retVal = DmaUtilsAutoInc3d_prepareTr(&trPrepParam, &transferProp);

   if (UDMA_SOK != retVal) {
      (void) printf("[Error] dma configure failed!!\n");
   }
   else {

      retVal = DmaUtilsAutoInc3d_configure(dmaUtilsContext, ch, pTrMemChs[ch], 1);
   }

   return retVal;
}

template <typename dType> uint32_t dmaUtils<dType>::trigger(uint8_t *dmaUtilsContext, int32_t *chId, size_t numChannels)
{

   for (size_t ch = 0; ch < numChannels - 1U; ch++) {
      (void) DmaUtilsAutoInc3d_trigger(dmaUtilsContext, chId[ch]);
   }

   return DmaUtilsAutoInc3d_trigger(dmaUtilsContext, chId[numChannels - 1U]);
}

template <typename dType>
uint32_t dmaUtils<dType>::trigger_single(uint8_t *dmaUtilsContext, int32_t *chId, int32_t channel)
{

   return DmaUtilsAutoInc3d_trigger(dmaUtilsContext, chId[channel]);
}

template <typename dType> void dmaUtils<dType>::wait(uint8_t *dmaUtilsContext, int32_t *chId, size_t numChannels)
{
   for (size_t ch = 0; ch < numChannels; ch++) {

      (void) DmaUtilsAutoInc3d_wait(dmaUtilsContext, chId[ch]);
   }

   return;
}

template <typename dType> void dmaUtils<dType>::wait_single(uint8_t *dmaUtilsContext, int32_t *chId, int32_t channel)
{

   (void) DmaUtilsAutoInc3d_wait(dmaUtilsContext, chId[channel]);

   return;
}

template <typename dType> uint32_t dmaUtils<dType>::get_tile_size(size_t width, size_t height, size_t l2Size)
{
   uint32_t maxTileSize = static_cast<uint32_t>(
       std::sqrt(static_cast<double>(l2Size / (2U * sizeof(dType)) + width * height)) - static_cast<double>(width));

   uint32_t recommended_tile_size = 1;
   while (recommended_tile_size < maxTileSize) {
      recommended_tile_size *= 2;
   }
   recommended_tile_size /= 2;

   if (recommended_tile_size > width) {
      recommended_tile_size = width;
   }

   return recommended_tile_size;
}

template class dmaUtils<float>;
template class dmaUtils<double>;
template class dmaUtils<int8_t>;
template class dmaUtils<uint8_t>;
template class dmaUtils<int16_t>;
template class dmaUtils<uint16_t>;
template class dmaUtils<int32_t>;
template class dmaUtils<uint32_t>;
template class dmaUtils<int64_t>;
template class dmaUtils<uint64_t>;

} // namespace TISP
