// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TI_TEST_H_
#define TI_TEST_H_ 1

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__C7100__) || defined(__C7120__) || defined(__C7502__) || defined(__C7504__) || defined(__C7508__)
#include <c7x.h>
#endif
#include "TI_cache.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(HOST_EMULATION)
// Takes a long time on HOST_EMULATION if num_reps are high; NUM_REPS is used to get average cycle count on target
#define NUM_REPS (4)
#else
#define NUM_REPS (100)
#endif

/* Number of significant digits in cycle formula display */
#define ti_PROFILE_SIG_DIGIT 5

/* Exclusive test parameters */
extern uint8_t TI_Verbose_Test_Level;
extern int32_t test_cases;
extern int32_t test_index;
extern int32_t act_kernel;
extern char    desc[];
extern char    TI_test_print_buffer[];
extern char    testPatternString[];

/* Kernel pass /fail enumeration */
enum { TI_TEST_KERNEL_FAIL = 0, TI_TEST_KERNEL_PASS = 1 };

static inline void TI_test_print_string(char *s)
{
#if !defined(NO_PRINTF)
#if defined(EVM_TEST)
   fputs(s, stdout);
#else
   fputs(s, stderr);
#endif
#endif
}

/* Generic Test Utility Prototypes */
void TI_test_print_float(float num, int printSign);
void TI_test_error_handler(void);

/* Standardized assertion utility */
void TI_test_assert(int assertion, char *err_string);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TI_TEST_H_ */
