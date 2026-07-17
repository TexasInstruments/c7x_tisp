// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_TEST_C7XECR_H_
#define TISP_TEST_C7XECR_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TISP_TEST_c7xSetTCR0(uint64_t param);

void TISP_TEST_c7xSetTBR0(uint64_t param);

void TISP_TEST_c7xSetMAR(uint64_t param);

void TISP_TEST_c7xSetSCR(uint64_t param);

// added during J7ES bring-up to perform TLB invalidate ALL after the MMU setup
// code
void TISP_TEST_c7xMmuTlbInvAll(void);

void TISP_TEST_c7xSetL2CFG(uint64_t param);

uint64_t TISP_TEST_c7xGetL2CFG(void);

void TISP_TEST_c7xSetL1DCFG(uint64_t param);

uint64_t TISP_TEST_c7xGetL1DCFG(void);

uint64_t MMALIB_c7xGetL1DWBINV(void);
uint64_t MMALIB_c7xGetL1DINV(void);
void     MMALIB_c7xSetL1DWBINV(uint64_t param);
void     MMALIB_c7xSetL1DINV(uint64_t param);
uint64_t MMALIB_c7xL1PCacheInvalidateAll();

#ifdef __cplusplus
}
#endif

#endif /* end of TISP_TEST_C7XECR_H_ definition */
