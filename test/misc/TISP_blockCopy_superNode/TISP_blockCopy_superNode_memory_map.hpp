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

#if SOC == AM275
#define WIDTH (512 >> 2)
#define HEIGHT (512 >> 2)
#define BLOCK_WIDTH (256 >> 2)
#define BLOCK_HEIGHT (32)
#else
#define WIDTH (2048)
#define HEIGHT (2048)
#define BLOCK_WIDTH (1024)
#define BLOCK_HEIGHT (32)

#endif

#define INPUT_ADDR (FFTLIB_F32 *) ddrBuffer
#define INPUT_CN_ADDR (INPUT_ADDR + (WIDTH * HEIGHT))
#define OUTPUT_ADDR (INPUT_CN_ADDR + (WIDTH * HEIGHT))
#define REF_OUT_ADDR (OUTPUT_ADDR + (WIDTH * HEIGHT))

#define INPUT_BLOCK_ADDR (float *) l2Buffer
#define OUTPUT_BLOCK_ADDR (INPUT_BLOCK_ADDR + 2 * BLOCK_HEIGHT * BLOCK_WIDTH)
