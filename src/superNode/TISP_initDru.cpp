// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <TISP_initDru.hpp>

#include "drivers/dmautils/src/dmautils_autoincrement_3d_priv.h"
#include <drivers/dmautils/dmautils.h>
#include <drivers/hw_include/csl_clec.h>
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/sciclient.h>
#include <drivers/udma.h>
#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/nortos/dpl/c75/csl_clec.h>

#define DRU_LOCAL_EVENT_START_DEFAULT (128U) /* Default for AM62A/AM62D/AM275X ref: clec spec */
#define DRU_NUM_OF_CHANNELS (16U)
#define APP_DMAUTILS_L2SRAM_SIZE (64 * 1024)
#define APP_DMAUTILS_ALIGN_CEIL(VAL, ALIGN) ((((VAL) + ((ALIGN) -1U)) / (ALIGN)) * (ALIGN))
#define APP_DMAUTILS_TESTAUTOINC_MAX_NUM_TR (32)
#define APP_DMAUTILS_ALIGN_SIZE (128U)

#if !defined(HOST_EMULATION) && __C7X_VEC_SIZE_BITS__ == 512
/*Configure CLEC*/
void TISP::appC7xClecInitDru(void)
{
   CSL_ClecEventConfig cfgClec;
   CSL_CLEC_EVTRegs   *clecBaseAddr =
       reinterpret_cast<CSL_CLEC_EVTRegs *>(CSL_COMPUTE_CLUSTER0_CLEC_REGS_BASE); // NOLINT(MISRA.CAST.INT_TO_PTR)
   uint32_t i               = 0;
   uint32_t dru_input_start = 192;
#if defined(SOC_J784S4)
   dru_input_start = DRU_LOCAL_EVENT_START_J784S4;
#else
   dru_input_start = DRU_LOCAL_EVENT_START_DEFAULT;
#endif
   uint32_t dru_input_num = 16;

   /*Only configuring 16 channels*/
   for (i = dru_input_start; i < (dru_input_start + dru_input_num); i++) {
      /* Configure CLEC */
      cfgClec.secureClaimEnable = FALSE;
      cfgClec.evtSendEnable     = TRUE;

      /* cfgClec.rtMap value is different for each C7x */
      cfgClec.rtMap = CSL_CLEC_RTMAP_CPU_4;

      cfgClec.extEvtNum = 0;
      cfgClec.c7xEvtNum = (i - dru_input_start) + UDMA_C7X_CORE_INTR_OFFSET;
      CSL_clecConfigEvent(clecBaseAddr, i, &cfgClec);
   }

   return;
}

int32_t TISP::test_sciclientDmscGetVersion(char *version_str, uint32_t version_str_size)
{
   int32_t retVal = 0;

   struct tisci_msg_version_req  request;
   const Sciclient_ReqPrm_t      reqPrm = {TISCI_MSG_VERSION, TISCI_MSG_FLAG_AOP,
                                           static_cast<uint8_t *>(static_cast<void *>(&request)), sizeof(request),
                                           SCICLIENT_SERVICE_WAIT_FOREVER};
   struct tisci_msg_version_resp response;
   Sciclient_RespPrm_t respPrm = {0, static_cast<uint8_t *>(static_cast<void *>(&response)), sizeof(response)};

   retVal = Sciclient_service(&reqPrm, &respPrm);
   if (0 == retVal) {
      if (respPrm.flags == TISCI_MSG_FLAG_ACK) {
         if (version_str == nullptr) {
            (void) printf("SCICLIENT: DMSC FW version [%s]\n", static_cast<char *>(static_cast<void *>(response.str)));
            (void) printf("SCICLIENT: DMSC FW revision 0x%x  \n", response.version);
            (void) printf("SCICLIENT: DMSC FW ABI revision %d.%d\n", response.abi_major, response.abi_minor);
         }
         else {
            (void) snprintf(version_str, version_str_size, "version %s, revision 0x%x, ABI %d.%d",
                            static_cast<char *>(static_cast<void *>(response.str)), response.version,
                            response.abi_major, response.abi_minor);
         }
      }
      else {
         retVal = -1;
      }
   }
   if (retVal != 0) {
      (void) printf("SCICLIENT: ERROR: DMSC Firmware Get Version failed !!!\n");
   }

   return retVal;
}

#endif

#if !defined(HOST_EMULATION)
// MISRA Deviation: MISRA.NS.GLOBAL (Required – MISRA C++ 2023 Rule 6.3)
// Justification: App_dmautilsClecInitDru is a public DMA utility API declared in
// TISP_initDru.hpp without namespace qualification. It is called directly by
// application-level initialisation code in the TI SDK ecosystem, which places DMA
// utility init functions at global scope by convention. Wrapping it in the TISP
// namespace would break backward compatibility with external callers that follow the
// TI SDK initialisation framework.
void App_dmautilsClecInitDru(void)
{
   // MISRA Deviation: MISRA.C_CAST (Required – MISRA C++ 2023 Rule 8.2.1)
   // Justification: CSL_CLEC_RTMAP_CPU_ALL is a TI CSL SDK macro defined in
   // csl_clec.h as ((uint32_t)((uint32_t) 0x000FU) << 2U), containing nested
   // C-style casts that cannot be modified in a third-party header. This constexpr
   // isolates the violation to a single definition point so all usage sites remain
   // free of C-style casts.
   constexpr uint32_t  kClecRtmapCpuAll = CSL_CLEC_RTMAP_CPU_ALL; // NOLINT(MISRA.C_CAST)
   CSL_ClecEventConfig cfgClec;
   CSL_CLEC_EVTRegs   *clecBaseAddr;
   uint32_t            clusterId;

   clusterId = CSL_clecGetC7xClusterId();

   // MISRA Deviation: AUTOSAR.CAST.REINTERPRET (Required – AUTOSAR C++14 A5-2-4)
   // Justification: CSL_C7X256V0_CLEC_BASE / CSL_C7X256V1_CLEC_BASE are integer
   // constants representing hardware MMIO register base addresses defined by the
   // TI CSL C SDK. Accessing MMIO registers requires converting an integer address
   // to a pointer; static_cast cannot perform integer-to-pointer conversion in C++,
   // so reinterpret_cast is the only valid mechanism. This is a one-time init call
   // executed before any real-time pipeline starts — no runtime aliasing or
   // undefined-behaviour concerns apply. This is the canonical pattern at the
   // C++ / C SDK hardware abstraction boundary.
   if (clusterId == CSL_C75_CPU_CLUSTER_NUM_C75_1) {
      clecBaseAddr = reinterpret_cast<CSL_CLEC_EVTRegs *>(CSL_C7X256V0_CLEC_BASE); // NOLINT(MISRA.CAST.INT_TO_PTR)
   }
#if (CSL_C7X256V_MAIN_CNT == 2U)
   else if (clusterId == CSL_C75_CPU_CLUSTER_NUM_C75_2) {
      clecBaseAddr = reinterpret_cast<CSL_CLEC_EVTRegs *>(CSL_C7X256V1_CLEC_BASE); // NOLINT(MISRA.CAST.INT_TO_PTR)
   }
#endif
   else {
      clecBaseAddr = nullptr;
   }

   if (clecBaseAddr != nullptr) {
      uint32_t i               = 0;
      uint32_t dru_input_start = DRU_LOCAL_EVENT_START_DEFAULT;
      uint32_t dru_input_num   = DRU_NUM_OF_CHANNELS;

      /* Only configuring 16 channels */
      for (i = dru_input_start; i < (dru_input_start + dru_input_num); i++) {
         /* Configure CLEC */
         cfgClec.secureClaimEnable = false;
         cfgClec.evtSendEnable     = true;
         cfgClec.rtMap             = kClecRtmapCpuAll;
         cfgClec.extEvtNum         = 0U;
         cfgClec.c7xEvtNum         = (i - dru_input_start) + UDMA_C7X_CORE_INTR_OFFSET;
         (void) CSL_clecConfigEvent(clecBaseAddr, i, &cfgClec);
      }
   }
}
#endif // !defined(HOST_EMULATION)
