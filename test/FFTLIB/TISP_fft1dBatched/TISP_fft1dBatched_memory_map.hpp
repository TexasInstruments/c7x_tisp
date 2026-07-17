// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <stdint.h>

#define NUM_POINTS (256)
#define NUM_CHANNELS (32)

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
#define DDR_SIZE (1 * 1024 * 1024) // 1 MB
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

#define INPUT_CN_ADDR (float *) ddrBuffer
#define FFT_CN_ADDR (INPUT_CN_ADDR + (NUM_POINTS << 1) * NUM_CHANNELS)
#define SQR_CN_ADDR (FFT_CN_ADDR + (NUM_POINTS << 1) * NUM_CHANNELS)
#define TW_CN_ADDR (SQR_CN_ADDR + NUM_POINTS * 2 * NUM_CHANNELS)

#define INPUT_BLOCK_ADDR (float *) l2Buffer
#define COPY_BLOCK_ADDR (INPUT_BLOCK_ADDR + +2 * NUM_POINTS * NUM_CHANNELS)
#define FFT_BLOCK_ADDR (COPY_BLOCK_ADDR + 2 * NUM_POINTS * NUM_CHANNELS)
#define SQR_BLOCK_ADDR (FFT_BLOCK_ADDR + 2 * NUM_POINTS * NUM_CHANNELS)
