// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_DMAUTILS_HPP
#define TISP_DMAUTILS_HPP

#include <cstdint>
#include <cstdlib>

#include "drivers/dmautils/src/dmautils_autoincrement_3d_priv.h"
#include <drivers/dmautils/dmautils.h>
#include <drivers/hw_include/csl_clec.h>
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/sciclient.h>
#include <drivers/udma.h>
#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/nortos/dpl/c75/csl_clec.h>

namespace TISP {

template <typename dType> class dmaUtils {

 public:
   static void dmaAutoIncSetupXferPropIn2D(size_t                         width,
                                           size_t                         height,
                                           size_t                         blockWidth,
                                           size_t                         blockHeight,
                                           size_t                         inPitch,
                                           size_t                         outPitch,
                                           DmaUtilsAutoInc3d_TransferDim *transferDim);
   static void dmaAutoIncSetupXferPropOut2D(size_t                         width,
                                            size_t                         height,
                                            size_t                         blockWidth,
                                            size_t                         blockHeight,
                                            size_t                         inPitch,
                                            size_t                         outPitch,
                                            DmaUtilsAutoInc3d_TransferDim *transferDim);

   static void dmaAutoIncSetupXferPropOut2DTranspose(size_t                         width,
                                                     size_t                         height,
                                                     size_t                         blockWidth,
                                                     size_t                         blockHeight,
                                                     size_t                         inPitch,
                                                     size_t                         outPitch,
                                                     DmaUtilsAutoInc3d_TransferDim *transferDim);

   static uint32_t init(uint8_t *dmaUtilsContext, int32_t numChannels, Udma_DrvHandle drvHandle);

   static uint32_t configure_channel(uint8_t                       *dmaUtilsContext,
                                     int32_t                        ch,
                                     uint8_t                      **pTrMemChs,
                                     uint8_t                       *srcPtr,
                                     uint8_t                       *dstPtr,
                                     uint32_t                       syncType,
                                     DmaUtilsAutoInc3d_TransferDim *transferDim);

   static uint32_t trigger(uint8_t *dmaUtilsContext, int32_t *chId, size_t numChannels);
   static void     wait(uint8_t *dmaUtilsContext, int32_t *chId, size_t numChannels);

   static uint32_t trigger_single(uint8_t *dmaUtilsContext, int32_t *chId, int32_t channel);
   static void     wait_single(uint8_t *dmaUtilsContext, int32_t *chId, int32_t channel);

   static uint32_t get_tile_size(size_t width, size_t height, size_t l2Size);

#if !defined(HOST_EMULATION) && __C7X_VEC_SIZE_BITS__ == 512
   static void    appC7xClecInitDru(void);
   static int32_t test_sciclientDmscGetVersion(char *version_str, uint32_t version_str_size);
#endif
};
} // namespace TISP
#endif
