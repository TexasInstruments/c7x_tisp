// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <dsplib.h>
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

#define NUM_CHANNELS (8)
#define NUM_CHANNELS_OUT (NUM_CHANNELS)
#define NUM_SAMPLES (4096)
#define BLOCK_SIZE (256)

#define INPUT_SIZE (BLOCK_SIZE * NUM_CHANNELS)
#define OUTPUT_SIZE (BLOCK_SIZE * NUM_CHANNELS_OUT)

#define INPUT_ADDR (float *) l2Buffer

#define INT_TO_FLOAT_ADDR (INPUT_ADDR + INPUT_SIZE)

// For cascadeBiquad
#define CASCADE_FORM (DSPLIB_NUM_STATE_VARS_DF1)
#define CASCADE_NUM_STAGES (3)
#define CASCADE_FILTER_SIZE (CASCADE_NUM_STAGES * 5)
#define CASCADE_FILTER_VAR_SIZE (CASCADE_NUM_STAGES * CASCADE_FORM * NUM_CHANNELS)

#define CASCADE0_COEFF_ADDR (INT_TO_FLOAT_ADDR + INPUT_SIZE)
#define CASCADE0_FILTER_VAR_ADDR (CASCADE0_COEFF_ADDR + CASCADE_FILTER_SIZE)
#define CASCADE0_OUT_ADDR (CASCADE0_FILTER_VAR_ADDR + CASCADE_FILTER_VAR_SIZE)

// For gainNCh
#define GAIN_NCH_TARGET_ADDR (CASCADE0_OUT_ADDR + INPUT_SIZE)
#define GAIN_NCH_OUT_ADDR (GAIN_NCH_TARGET_ADDR + NUM_CHANNELS)

// For muteNCh
#define MUTE_NCH_INITIAL_GAIN_ADDR (GAIN_NCH_OUT_ADDR + INPUT_SIZE)
#define MUTE_NCH_TARGET_GAIN_ADDR (MUTE_NCH_INITIAL_GAIN_ADDR + NUM_CHANNELS)
// Ensure proper alignment for int32_t array
#define MUTE_NCH_IS_MUTE_ADDR (MUTE_NCH_TARGET_GAIN_ADDR + NUM_CHANNELS + (NUM_CHANNELS % 2))
#define MUTE_NCH_OUT_ADDR (MUTE_NCH_IS_MUTE_ADDR + NUM_CHANNELS * sizeof(int32_t) / sizeof(float) + (NUM_CHANNELS % 2))

// For final typeConversion
#define FLOAT_TO_INT_ADDR (MUTE_NCH_OUT_ADDR + INPUT_SIZE)
