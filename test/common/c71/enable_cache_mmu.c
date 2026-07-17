// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/* ============================================================================
 * PROJECT SPECIFIC INCLUDE FILES
 * =============================================================================
 */
#include "enable_cache_mmu.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void enable_cache_mmu(uint64_t ttbr)
{
#if !defined(_HOST_BUILD)
   uint64_t tcr;
   uint64_t mar;
   uint64_t scr;

   tcr = 0x0000000000002a21U;
   scr = 0x80000000000000c1U;
   mar = 0x3D3D3D2915032A00U;

   TISP_TEST_c7xSetTCR0(tcr);  // ECR785
   TISP_TEST_c7xSetTBR0(ttbr); // ECR787
   TISP_TEST_c7xSetMAR(mar);   // ECR789
   TISP_TEST_c7xSetSCR(scr);   // ECR784  *** This register has to be last ***
#endif
}

#ifdef __cplusplus
}
#endif // extern "C"
