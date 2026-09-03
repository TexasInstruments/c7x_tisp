// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TI_PROFILE_H_
#define TI_PROFILE_H_ 1

/* Memory utility includes */
#include "TI_test.h"
#include <c7x.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Global test case index */
extern int test_index; /* Index of current active test */
extern int act_kernel; /* Active kernel type for test and profile */

/* Kernel routine type enumeration (must not use explicit numbering) */
#define TI_PROFILE_KERNEL_OPT 0      /* Kernel type optimized (C-intrinsics, assembly) */
#define TI_PROFILE_KERNEL_OPT_WARM 1 /* Kernel type optimized (C-intrinsics, assembly) with warm instruction cache */
#define TI_PROFILE_KERNEL_OPT_WARMWRB                                                                                  \
   2 /* Kernel type optimized (C-intrinsics, assembly) with warm instruction cache, warm output cache */
#define TI_PROFILE_KERNEL_CN 3     /* Kernel type natural C */
#define TI_PROFILE_KERNEL_INIT 4   /* Kernel init for C6x, C7x */
#define TI_PROFILE_KERNEL_CNINIT 5 /* Kernel init for natural C */
#define TI_PROFILE_KERNEL_CNT 6

/* Profiling defines */
#define TI_PROFILE_MAX_TESTS 32
#define TI_PROFILE_MAX_ORDER 4
#define TI_PROFILE_MAX_MODES 6
#define TI_PROFILE_FORMULA 0
#define TI_PROFILE_RANGE 1
#define TI_PROFILE_FORMULA_RANGE 2

/* Profiling macros */
#ifdef _HOST_BUILD
#define PROFILE_INIT
#define PROFILE_READ 0
#else
#if defined(__C7100__) || defined(__C7120__) || defined(__C7502__) || defined(__C7504__) || defined(__C7508__) ||      \
    defined(__C7524__)
#define PROFILE_INIT
#define PROFILE_READ (__get_GTSC(1), __STSC)
#else
#define PROFILE_INIT TSCL = 0, TSCH = 0
#define PROFILE_READ _itoll(TSCH, TSCL)
#endif
#endif

/* Profiling globals */
extern uint64_t beg_count;                     /* Begin cycle count for profiling */
extern uint64_t end_count;                     /* End cycle count for profiling */
extern uint64_t overhead;                      /* Cycle profiling overhead */
extern uint64_t cycles[TI_PROFILE_KERNEL_CNT]; /* Cycle counts for C-natural & optimized */

void     TI_profile_init(const char *kernel_name);
int64_t  TI_profile_get_cycles(int kernel);
void     TI_profile_add_test(uint32_t testNum,
                             int      Ax0,
                             uint64_t arch_cycles,
                             uint64_t est_cycles,
                             int      errorFlag,
                             char    *desc);
void     TI_profile_clear_run_stats();
void     TI_profile_skip_test(char *desc);
void     TI_profile_cycle_report(int reportType, char *formulaInfo, char *rangeInfo);
void     TI_profile_initStack(uint32_t SP);
void     TI_profile_setStackDepth(void);
uint32_t TI_profile_getSP(void);
void     TI_profile_stack_memory(void);
void     TI_profile_clear_cycle_counts(void);
void     TI_profile_clear_stack_sizes(void);
void     TI_profile_clear_cycle_count_single(int kernel);
float    TI_Profile_get_snr(void *pX, void *pY, int outputSize);
float    TI_Profile_get_log_snr(void *pX, void *pY, int outputSize, float logOffset);

/* Profiling start routine */
static inline void TI_profile_start(int kernel)
{
   /* Set the active kernel for subsequent calls */
   act_kernel = kernel;

   /* Invalidate the cache */
   // TI_cache_inval();

#if (!defined(RTL_TEST) && !defined(QT_TEST) && !defined(EVM_TEST))
   /* Initialize the stack for stack depth profiling */
   TI_profile_initStack(TI_profile_getSP());
#endif

   /* Register starting timestamp */
   beg_count = PROFILE_READ;
}

/* Profiling stop (and calculate) routine */
static inline void TI_profile_stop(void)
{
   /* Register ending timestamp */
   end_count = PROFILE_READ;

   /* Calculate and store cycles */
   cycles[act_kernel] += end_count - beg_count - overhead;

#if (!defined(RTL_TEST) && !defined(QT_TEST) && !defined(EVM_TEST))
   /* Analize the stack and register stack depth used */
   TI_profile_setStackDepth();
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
