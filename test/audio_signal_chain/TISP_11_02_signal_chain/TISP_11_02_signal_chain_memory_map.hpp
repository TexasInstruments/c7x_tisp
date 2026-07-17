// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <audiolib.h>
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
// TODO: increase DDR
#define DDR_SIZE (64 * 1024 * 1024)
// #define DDR_SIZE (1 * 1024 * 1024) // 4 MB
#else
#define DDR_SIZE (64 * 1024 * 1024) // 64 MB
#endif

#if SOC == j722s || SOC == AM275
// TODO: increase L2
#define L2SRAM_SIZE (1 * 1024 * 1024)
// #define L2SRAM_SIZE (1024 * 1024) // 1 MB
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

// Signal chain parameters
#define SAMPLING_RATE_HZ (48000)
#define NUM_CHANNELS_IN (8)
#define NUM_CHANNELS_OUT (12)
#define NUM_SAMPLES (4096)
#define BLOCK_SIZE (64)
#define IS_INTERLEAVED (1)
#define SIMD_WIDTH (8)
#define AUDIOLIB_ROW_STRIDE(x, y) (((x + y) / y) * y)

#define INPUT_SIZE (BLOCK_SIZE * NUM_CHANNELS_IN)
#define OUTPUT_SIZE (BLOCK_SIZE * NUM_CHANNELS_OUT)

// Memory allocation for input and output
#define INPUT_ADDR (int32_t *) l2Buffer

// 1. typeconversion (int32_t to float)
#define TYPE_CONV_IN_OUT_ADDR (float *) (INPUT_ADDR + INPUT_SIZE)

// 2. gainNCh
#define GAIN_NCH_TARGET_ADDR (TYPE_CONV_IN_OUT_ADDR + INPUT_SIZE)
#define GAIN_NCH_OUT_ADDR (GAIN_NCH_TARGET_ADDR + NUM_CHANNELS_IN)

// 3. cascadeBiquad
#define CASCADE_FORM (DSPLIB_NUM_STATE_VARS_DF1)
#define CASCADE_NUM_STAGES (3)
#define CASCADE_FILTER_SIZE (CASCADE_NUM_STAGES * 5)
#define CASCADE_FILTER_VAR_SIZE (CASCADE_NUM_STAGES * CASCADE_FORM * NUM_CHANNELS_IN)

#define CASCADE_COEFF_ADDR (GAIN_NCH_OUT_ADDR + INPUT_SIZE)
#define CASCADE_FILTER_VAR_ADDR (CASCADE_COEFF_ADDR + CASCADE_FILTER_SIZE)
#define CASCADE_OUT_ADDR (CASCADE_FILTER_VAR_ADDR + CASCADE_FILTER_VAR_SIZE)

// 4. router (8ch to 12ch)
#define ROUTER_DIMY_PADDED (AUDIOLIB_ROW_STRIDE(BLOCK_SIZE, (2 * SIMD_WIDTH)))
#define ROUTER_STRIDE_IN_ELEM                                                                                          \
   ((AUDIOLIB_CALC_STRIDE(NUM_CHANNELS_IN * sizeof(AUDIOLIB_FLOAT32), AUDIOLIB_ALIGN_SHIFT_64BYTES)) /                 \
    sizeof(AUDIOLIB_FLOAT32)) // aligned strideOutElements
#define ROUTER_STRIDE_OUT_ELEM                                                                                         \
   ((AUDIOLIB_CALC_STRIDE(NUM_CHANNELS_OUT * sizeof(AUDIOLIB_FLOAT32), AUDIOLIB_ALIGN_SHIFT_64BYTES)) /                \
    sizeof(AUDIOLIB_FLOAT32)) // aligned strideOutElements
#define ROUTER_OUT_CHANNELS_ADDR (CASCADE_OUT_ADDR + BLOCK_SIZE * NUM_CHANNELS_IN)
#define ROUTER_SCRATCH_ADDR (ROUTER_OUT_CHANNELS_ADDR + NUM_CHANNELS_IN)
#define ROUTER_OUT_ADDR (ROUTER_SCRATCH_ADDR + NUM_CHANNELS_OUT * BLOCK_SIZE)

// 5. parametric eq
// Using the same coefficients as cascadeBiquad
#define PARAM_EQ_FILTER_VAR_SIZE (CASCADE_NUM_STAGES * CASCADE_FORM * NUM_CHANNELS_OUT)
#define PARAM_EQ_FILTER_VAR_ADDR (ROUTER_OUT_ADDR + BLOCK_SIZE * NUM_CHANNELS_OUT)
#define PARAM_EQ_OUT_ADDR (PARAM_EQ_FILTER_VAR_ADDR + PARAM_EQ_FILTER_VAR_SIZE)

// 6. delayNCh
#define DELAY_MAX_DELAY (0) // max delay of all channels
#define DELAY_NCH_LINEAR_MODE (0)
#define DELAY_NCH_SIZES_ADDR (PARAM_EQ_OUT_ADDR + BLOCK_SIZE * NUM_CHANNELS_OUT)
// For linear mode (mode=0), buffer size is NUM_CHANNELS_OUT * (DELAY_MAX_DELAY)
#define DELAY_BUFFER_SIZE (NUM_CHANNELS_OUT * (DELAY_MAX_DELAY + 1))
#define DELAY_NCH_BUFFER_ADDR (DELAY_NCH_SIZES_ADDR + NUM_CHANNELS_OUT)
// Scratch buffer needs to be BLOCK_SIZE * NUM_CHANNELS_OUT
#define DELAY_NCH_SCRATCH_ADDR (DELAY_NCH_BUFFER_ADDR + DELAY_BUFFER_SIZE)
#define DELAY_NCH_OUT_ADDR (DELAY_NCH_SCRATCH_ADDR + BLOCK_SIZE * NUM_CHANNELS_OUT)

// 7. balanceFader
#define BALANCE_FADER_GAIN_ADDR (DELAY_NCH_OUT_ADDR + BLOCK_SIZE * NUM_CHANNELS_OUT)
#define BALANCE_FADER_CHANNEL_CONFIG_ADDR (BALANCE_FADER_GAIN_ADDR + NUM_CHANNELS_OUT)
#define BALANCE_FADER_OUT_ADDR (BALANCE_FADER_CHANNEL_CONFIG_ADDR + NUM_CHANNELS_OUT)

// 8. gainNChTrim
#define GAIN_NCH_TRIM_CHANNEL_GAINS_ADDR (BALANCE_FADER_OUT_ADDR + BLOCK_SIZE * NUM_CHANNELS_OUT)
#define GAIN_NCH_TRIM_MASTER_GAIN_ADDR (GAIN_NCH_TRIM_CHANNEL_GAINS_ADDR + NUM_CHANNELS_OUT)
#define GAIN_NCH_TRIM_OUT_ADDR (GAIN_NCH_TRIM_MASTER_GAIN_ADDR + 1)

// 9. limiter
#define LIMITER_GAIN_ADDR (GAIN_NCH_TRIM_OUT_ADDR + BLOCK_SIZE * NUM_CHANNELS_OUT)
#define LIMITER_SCRATCH_ADDR (LIMITER_GAIN_ADDR + NUM_CHANNELS_OUT)
#define LIMITER_THRESHOLD_ADDR (LIMITER_SCRATCH_ADDR + BLOCK_SIZE * NUM_CHANNELS_OUT)
#define LIMITER_KNEE_WIDTH_ADDR (LIMITER_THRESHOLD_ADDR + NUM_CHANNELS_OUT)
#define LIMITER_ATTACK_COEFF_ADDR (LIMITER_KNEE_WIDTH_ADDR + NUM_CHANNELS_OUT)
#define LIMITER_RELEASE_COEFF_ADDR (LIMITER_ATTACK_COEFF_ADDR + NUM_CHANNELS_OUT)
#define LIMITER_OUT_ADDR (LIMITER_RELEASE_COEFF_ADDR + NUM_CHANNELS_OUT)

// 10. muteNCh
#define MUTE_NCH_STRIDE_ELEM                                                                                           \
   ((AUDIOLIB_CALC_STRIDE(NUM_CHANNELS_OUT * sizeof(AUDIOLIB_FLOAT32), AUDIOLIB_ALIGN_SHIFT_64BYTES)) /                \
    sizeof(AUDIOLIB_FLOAT32)) // aligned strideOutElements
#define MUTE_NCH_IS_MUTE_ADDR (LIMITER_OUT_ADDR + MUTE_NCH_STRIDE_ELEM * BLOCK_SIZE)
#define MUTE_NCH_OUT_ADDR (MUTE_NCH_IS_MUTE_ADDR + NUM_CHANNELS_OUT)
#define MUTE_NCH_FADE_TIME (0.0f)
#define MUTE_NCH_FADE_HARD_TYPE (2)

// 11. typeconversion (float to int32_t)
#define FLOAT_TO_INT_ADDR (int32_t *) (MUTE_NCH_OUT_ADDR + BLOCK_SIZE * MUTE_NCH_STRIDE_ELEM)
