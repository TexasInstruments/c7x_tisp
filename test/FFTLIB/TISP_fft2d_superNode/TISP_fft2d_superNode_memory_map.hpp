// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#if defined(HOST_EMULATION)
#include <malloc.h>
#elif __C7X_VEC_SIZE_BITS__ == 512
#include <ti/csl/arch/c7x/cslr_C7X_CPU.h>
#include <ti/csl/csl_clec.h>
#else
#include <drivers/hw_include/csl_clec.h>
#endif

#if SOC == AM275
#define NUM_POINTS (256)
#define NUM_CHANNELS (64)
#define BLOCK_WIDTH (NUM_POINTS << 1)
#define BLOCK_HEIGHT (16)
#else
#define NUM_POINTS (512)
#define NUM_CHANNELS (512)
#define BLOCK_WIDTH (NUM_POINTS << 1)
#define BLOCK_HEIGHT (16)
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

#define INPUT_ADDR (FFTLIB_F32 *) ddrBuffer
#define INPUT_CN_ADDR (INPUT_ADDR + (NUM_POINTS << 1) * NUM_CHANNELS)
#define OUTPUT_ADDR (INPUT_CN_ADDR + (NUM_POINTS << 1) * NUM_CHANNELS)
#define REF_OUT_ADDR (OUTPUT_ADDR + (NUM_POINTS << 1) * NUM_CHANNELS)
#define TEMP_OUT_ADDR (REF_OUT_ADDR + (NUM_POINTS << 1) * NUM_CHANNELS)
#define TW_CN_ADDR (TEMP_OUT_ADDR + NUM_POINTS * 2 * NUM_CHANNELS)
#define TEMP_OUT_CN_ADDR (TW_CN_ADDR + NUM_POINTS * 2 * NUM_CHANNELS)

#define INPUT_BLOCK_ADDR (float *) l2Buffer
#define TEMP_BLOCK_ADDR (INPUT_BLOCK_ADDR + 2 * BLOCK_HEIGHT * BLOCK_WIDTH)
#define OUT_BLOCK_ADDR (TEMP_BLOCK_ADDR + 2 * BLOCK_HEIGHT * BLOCK_WIDTH)
#define TW_ADDR (OUT_BLOCK_ADDR + 2 * BLOCK_HEIGHT * BLOCK_WIDTH)
