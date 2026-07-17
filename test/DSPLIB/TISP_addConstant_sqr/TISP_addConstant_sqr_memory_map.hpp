// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <stdint.h>

#if defined(HOST_EMULATION)
#include <malloc.h>
#elif __C7X_VEC_SIZE_BITS__ == 512
#include <ti/csl/arch/c7x/cslr_C7X_CPU.h>
#include <ti/csl/csl_clec.h>
#else
#include <drivers/hw_include/csl_clec.h>
#endif

#if SOC == AM275
// DDR is OCSRAM here
#define DDR_SIZE (1 * 1024 * 1024) // 4 MB
#else
#define DDR_SIZE (64 * 1024 * 1024) // 64 MB
#endif

#if SOC == j722s || SOC == AM275
#define L2SRAM_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || SOC == AM62D
#define L2SRAM_SIZE (800 * 1024) // 800 KB
#endif

#if defined(HOST_EMULATION)
uint8_t ddrBuffer[DDR_SIZE];
uint8_t l2Buffer[L2SRAM_SIZE];
#endif

#if !defined(HOST_EMULATION)
__attribute__((section(".ddrData"), aligned(128))) uint8_t    ddrBuffer[DDR_SIZE];
__attribute__((section(".l2sramData"), aligned(128))) uint8_t l2Buffer[L2SRAM_SIZE];
#endif

#define BLOCK_SIZE (65536)

#define INPUT1_CN_ADDR (float *) ddrBuffer
#define OUTPUT0_CN_ADDR ((INPUT1_CN_ADDR) + (BLOCK_SIZE))
#define OUTPUT1_CN_ADDR (OUTPUT0_CN_ADDR + (BLOCK_SIZE))

#define INPUT0_ADDR (float *) l2Buffer
#define INPUT1_ADDR (INPUT0_ADDR + 128) // memory alignment
#define OUTPUT0_ADDR (INPUT1_ADDR + BLOCK_SIZE)
#define OUTPUT1_ADDR (OUTPUT0_ADDR + BLOCK_SIZE)
