// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/* Profiling includes */
#include "TI_profile.h"
#include "TI_memory.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static uint16_t testId;

/* Handle COFF/ELF for linker cmd file symbols */
#if defined(__TI_EABI__)
#define _STACK_SIZE __TI_STACK_SIZE
#endif

/* Stack size global extern variables */
extern uint32_t _STACK_SIZE;
extern uint32_t _stack;

/* Stack size static variables */
static uint32_t stackSize[TI_PROFILE_KERNEL_CNT];    /* Stack size for C-natural
                                                        and optimized */
static uint32_t maxStackSize[TI_PROFILE_KERNEL_CNT]; /* Max stack sizes for C-natural and
                                                        optimized */
#if !defined(_HOST_BUILD) && !defined(DISABLE_STACK_PROFILING)
static uint32_t  oldSP, newSP;
static uint32_t *topStack = &_stack;
#endif

/* Profiling globals */
uint64_t beg_count;                     /* Begin cycle count for profiling */
uint64_t end_count;                     /* End cycle count for profiling */
uint64_t overhead;                      /* Cycle profiling overhead */
uint64_t cycles[TI_PROFILE_KERNEL_CNT]; /* Cycle counts for C-natural and
                                           optimized */

/* File internal helper functions */
void TI_profile_clear_all_stats();

/* Profiling initialization routine */

void TI_profile_init(const char *kernel_name)
{

   PROFILE_INIT;

   /* Compute the overhead of calling clock twice to get timing info. */
   beg_count = PROFILE_READ;
   end_count = PROFILE_READ;
   overhead  = end_count - beg_count;

   /* Clear cycle counts */
   /* Clear stack sizes */
   /* Clear max stack sizes */
   TI_profile_clear_all_stats();

   testId = 0;
#if !defined(NO_PRINTF)
   /* Informational message */
   sprintf(TI_test_print_buffer, "----------------------------------------------"
                                 "------------------------------"
                                 "----------------------------------------------"
                                 "------------------------------"
                                 "-------------------------\n");
   TI_test_print_string(TI_test_print_buffer);
   sprintf(TI_test_print_buffer, "               %s testing starts.\n", kernel_name);
   TI_test_print_string(TI_test_print_buffer);
   sprintf(TI_test_print_buffer, "----------------------------------------------"
                                 "------------------------------"
                                 "----------------------------------------------"
                                 "------------------------------"
                                 "-------------------------\n");
   TI_test_print_string(TI_test_print_buffer);
   sprintf(TI_test_print_buffer, "| No  | ID  | Status | Num pt  | Kernel Init   | Kernel Compute  | NatC "
                                 "Compute  | Arch. Compute | Efficiency  | "
                                 "Est. "
                                 " Compute | Accuracy    | Description\n");
   TI_test_print_string(TI_test_print_buffer);
   sprintf(TI_test_print_buffer, "|     |     |        |         |  cyc          |  cyc            |  cyc   "
                                 "    "
                                 "   | cyc (est.)    | vs Arch.(%%) | cyc "
                                 "(est.)    | vs Est.(%%)  |            \n");
   TI_test_print_string(TI_test_print_buffer);
   sprintf(TI_test_print_buffer, "----------------------------------------------"
                                 "------------------------------"
                                 "----------------------------------------------"
                                 "------------------------------"
                                 "-------------------------\n");
   TI_test_print_string(TI_test_print_buffer);
#endif
}

#define MAX_LINELEN 60
#define MAX_NUMLINES 3

int64_t TI_profile_get_cycles(int kernel) { return cycles[kernel]; }

/* Profiling add calculation */
void TI_profile_add_test(uint32_t testNum,
                         int      Ax0,
                         uint64_t arch_cycles,
                         uint64_t est_cycles,
                         int      errorFlag,
                         char    *desc)
{
#if !defined(NO_PRINTF)
   uint16_t index;
   //#if !defined(EVM_TEST)
   uint16_t maxIndex;
   uint64_t compute_cycles;
   //#endif
   char *descString;
   char *curStr;
   char *line[MAX_NUMLINES + 1];
   char *emptyString = "";

   index = 1;

   if (desc == NULL) {
      descString = emptyString;
      line[0]    = descString;
   }
   else {
      descString = desc;
      /* Code below is to break up a long description string into multiple lines
       * so everything fits nicely in the display.
       * The algorithm looks for last space before the line exceeeds
       * MAX_LINELEN characters to determine end of each line
       */
      curStr  = descString;
      line[0] = descString;
      index   = 1;

      while ((strlen(curStr) > MAX_LINELEN) && (index < MAX_NUMLINES)) {
         curStr += MAX_LINELEN;

         while (*curStr != ' ' && curStr != descString) {
            curStr--;
         }

         if (curStr != descString) {
            line[index++] = curStr;
         }
         else {
            line[index++] = descString + strlen(descString);
         }
         curStr++;
      }
   }

   //#if !defined(EVM_TEST)
   maxIndex = index;
   //#endif
   line[index] = descString + strlen(descString);

   index = 0;

   *(line[index + 1]) = 0; /* we set the end of line to '0' */
   // compute_cycles= (float)cycles[TI_PROFILE_KERNEL_OPT] / Ax0;
   testId++;

   //#if !defined(EVM_TEST)
   compute_cycles = cycles[TI_PROFILE_KERNEL_OPT];
   if (!errorFlag) {
      sprintf(TI_test_print_buffer, "|%4d |%4d | PASS", testId, testNum);
      TI_test_print_string(TI_test_print_buffer);
   }
   else {
      sprintf(TI_test_print_buffer, "|%4d |%4d | FAIL", testId, testNum);
      TI_test_print_string(TI_test_print_buffer);
   }

   sprintf(TI_test_print_buffer,
           "   | %7d |    %10lu |    %12lu |    %10lu |   "
           " %10lu "
           "|    %8.0f |    %10lu |    %8.0f |",
           Ax0, cycles[TI_PROFILE_KERNEL_INIT], compute_cycles, cycles[TI_PROFILE_KERNEL_CN], arch_cycles,
           100 * ((float) arch_cycles / compute_cycles), est_cycles, 100 * ((float) est_cycles / compute_cycles));
   TI_test_print_string(TI_test_print_buffer);
   sprintf(TI_test_print_buffer, " %s\n", line[0]);
   TI_test_print_string(TI_test_print_buffer);
   if (index != maxIndex - 1) {
      *(line[index + 1]) = ' '; /* we set back the end of line to ' ' if not last line */
   }
   index++;
   line[index]++; /* advance pointer line[index] by 1 to skip the 0 */

   while (index != maxIndex) {
      *(line[index + 1]) = 0;
      sprintf(TI_test_print_buffer,
              "|     |     |        |         |  "
              "       "
              "      |                 |               |               |             "
              "|      "
              "         |             | %s\n",
              line[index]);
      TI_test_print_string(TI_test_print_buffer);
      if (index != maxIndex - 1) {
         *(line[index + 1]) = ' ';
      }
      index++;
      line[index]++; /* advance pointer line[index] by 1 to skip the 0 */
   }
//#elif defined(EVM_TEST)
//  if (!errorFlag) {
//            "%5d, PASS, Cold Cycles = %6d, Warm Cycles = %6d, Warm Cycles WRB "
//            "= %6d\n",
//            testNum, (int32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT),
//            (int32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM),
//            (int32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB));
//    TI_test_print_string(TI_test_print_buffer);
//  } else {
//            "%5d, FAIL, Cold Cycles = %6d, Warm Cycles = %6d, Warm Cycles WRB "
//            "= %6d\n",
//            testNum, (int32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT),
//            (int32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM),
//            (int32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB));
//    TI_test_print_string(TI_test_print_buffer);
//  }
//#endif // EVM_TEST
#endif // NO_PRINTF

   if (stackSize[TI_PROFILE_KERNEL_OPT] > maxStackSize[TI_PROFILE_KERNEL_OPT]) {
      maxStackSize[TI_PROFILE_KERNEL_OPT] = stackSize[TI_PROFILE_KERNEL_OPT];
   }
   if (stackSize[TI_PROFILE_KERNEL_CN] > maxStackSize[TI_PROFILE_KERNEL_CN]) {
      maxStackSize[TI_PROFILE_KERNEL_CN] = stackSize[TI_PROFILE_KERNEL_CN];
   }

   TI_profile_clear_run_stats();
}

void TI_profile_clear_cycle_counts()
{

   /* Clear cycle counts for next test */
   cycles[TI_PROFILE_KERNEL_OPT]         = (uint64_t) 0;
   cycles[TI_PROFILE_KERNEL_OPT_WARM]    = (uint64_t) 0;
   cycles[TI_PROFILE_KERNEL_OPT_WARMWRB] = (uint64_t) 0;
   cycles[TI_PROFILE_KERNEL_CN]          = (uint64_t) 0;
   cycles[TI_PROFILE_KERNEL_INIT]        = (uint64_t) 0;
}

void TI_profile_clear_stack_sizes()
{
   /* Clear stack sizes */
   stackSize[TI_PROFILE_KERNEL_OPT]         = (uint32_t) 0;
   stackSize[TI_PROFILE_KERNEL_OPT_WARM]    = (uint32_t) 0;
   stackSize[TI_PROFILE_KERNEL_OPT_WARMWRB] = (uint32_t) 0;
   stackSize[TI_PROFILE_KERNEL_CN]          = (uint32_t) 0;
   stackSize[TI_PROFILE_KERNEL_INIT]        = (uint32_t) 0;
}

void TI_profile_clear_cycle_count_single(int kernel) { cycles[kernel] = (uint64_t) 0; }

void TI_profile_clear_max_stack_sizes()
{
   /* Clear maximum stack sizes */
   maxStackSize[TI_PROFILE_KERNEL_OPT]         = (uint32_t) 0;
   maxStackSize[TI_PROFILE_KERNEL_OPT_WARM]    = (uint32_t) 0;
   maxStackSize[TI_PROFILE_KERNEL_OPT_WARMWRB] = (uint32_t) 0;
   maxStackSize[TI_PROFILE_KERNEL_CN]          = (uint32_t) 0;
   maxStackSize[TI_PROFILE_KERNEL_INIT]        = (uint32_t) 0;
}

void TI_profile_clear_run_stats()
{
   TI_profile_clear_cycle_counts();
   TI_profile_clear_stack_sizes();
}

void TI_profile_clear_all_stats()
{
   TI_profile_clear_cycle_counts();
   TI_profile_clear_stack_sizes();
   TI_profile_clear_max_stack_sizes();
}

/* Profiling skip message */
void TI_profile_skip_test(char *desc)
{
   uint16_t index, maxIndex;
   char    *descString;
   char    *curStr;
   char    *line[MAX_NUMLINES + 1];
   char    *emptyString = "";
   uint32_t heapSize;

   index = 1;

   if (desc == NULL) {
      descString = emptyString;
      line[0]    = descString;
   }
   else {
      descString = desc;
      /* Code below is to break up a long description string into multiple lines
       * so everything fits nicely in the display.
       * The algorithm looks for last space before the line exceeeds
       * MAX_LINELEN characters to determine end of each line
       */
      curStr  = descString;
      line[0] = descString;
      index   = 1;

      while ((strlen(curStr) > MAX_LINELEN) && (index < MAX_NUMLINES)) {
         curStr += MAX_LINELEN;

         while (*curStr != ' ' && curStr != descString) {
            curStr--;
         }

         if (curStr != descString) {
            line[index++] = curStr;
         }
         else {
            line[index++] = descString + strlen(descString);
         }
         curStr++;
      }
   }

   maxIndex    = index;
   line[index] = descString + strlen(descString);

   index = 0;

   *(line[index + 1]) = 0; /* we set the end of line to '0' */
   heapSize           = TI_get_heap_size();
   sprintf(TI_test_print_buffer,
           "|%3d | Test vector data buffers do not fit in L2RAM heap (%6d "
           "bytes) test "
           "skipped |",
           testId++, heapSize);
   TI_test_print_string(TI_test_print_buffer);
   sprintf(TI_test_print_buffer, " %s\n", line[0]);
   TI_test_print_string(TI_test_print_buffer);
   if (index != maxIndex - 1) {
      *(line[index + 1]) = ' '; /* we set back the end of line to ' ' if not last line */
   }
   index++;
   line[index]++; /* advance pointer line[index] by 1 to skip the 0 */

   while (index != maxIndex) {
      *(line[index + 1]) = 0;
      sprintf(TI_test_print_buffer,
              "|    |                                                            "
              "       "
              "            | %s\n",
              line[index]);
      TI_test_print_string(TI_test_print_buffer);
      if (index != maxIndex - 1) {
         *(line[index + 1]) = ' ';
      }
      index++;
      line[index]++; /* advance pointer line[index] by 1 to skip the 0 */
   }
}

uint32_t TI_profile_getSP(void)
{
#if !defined(_HOST_BUILD) && !defined(DISABLE_STACK_PROFILING)
   /* Must use static variable since I can't return pointer to local variable */
   static uint32_t *pTemp = 0;

   uint32_t iTemp; /* Local variable on stack, within 8 bytes of calling function */

   pTemp = &iTemp; /* Assign address of local variable to static location in
                      fardata section */

   return ((uint64_t) pTemp); /* Return address of variable near top of stack */
#else
   return (0);
#endif
}

void TI_profile_initStack(uint32_t SP)
{
#if !defined(_HOST_BUILD) && !defined(DISABLE_STACK_PROFILING)
   int32_t  i;
   uint32_t stsize;

#if defined(_TMS320C6600)
#define SP_SKIP 16
#else
#define SP_SKIP 8
#endif

   oldSP  = SP;
   stsize = oldSP - (uint64_t) topStack - SP_SKIP;

   for (i = 0; i < stsize / 4; i++) {
      topStack[i] = 0xDEADBEEF;
   }

#endif
}

void TI_profile_setStackDepth(void)
{
#if !defined(_HOST_BUILD) && !defined(DISABLE_STACK_PROFILING)
   int32_t i;

   for (i = 0; i < ((uint64_t) &_STACK_SIZE / 4); i++) {
      if (topStack[i] != 0xDEADBEEF) {
         break;
      }
   }

   newSP = (i * 4) + (uint64_t) topStack;

   stackSize[act_kernel] = (oldSP - newSP);
#endif
}

/* Display stack memory reqirements */
void TI_profile_stack_memory(void)
{
#if !defined(_HOST_BUILD) && !defined(DISABLE_STACK_PROFILING)
   /* Provide memory information */
   sprintf(TI_test_print_buffer, "  Stack usage:  %7d bytes\n", maxStackSize[TI_PROFILE_KERNEL_OPT]);
   TI_test_print_string(TI_test_print_buffer);
#endif
}

float TI_Profile_get_snr(void *pX, void *pY, int outputSize)
{
   /* Testing parameters */
   float  snr    = 0.0;
   double sumSqX = 0.0, sumSqY = 0.0;
   int    ix;
   float *pRef    = (float *) pX;
   float *pTarget = (float *) pY;
   for (ix = 0; ix < outputSize; ix++) {
      sumSqX = sumSqX + ((float) pRef[ix] * (float) pRef[ix]);
      sumSqY = sumSqY + ((float) (pRef[ix] - (float) pTarget[ix]) * ((float) pRef[ix] - (float) pTarget[ix]));
   }
   if (sumSqX != 0.0f && sumSqY != 0.0f && isfinite(sumSqX) && isfinite(sumSqY)) {
      /* Normal case: both accumulators are finite and non-zero.
       * Standard SNR formula: 10 * log10(signal_power / noise_power) */
      snr = 10.0 * log10(sumSqX / sumSqY);
   }
   return snr;
}

float TI_Profile_get_log_snr(void *pX, void *pY, int outputSize, float logOffset)
{
   /* Testing parameters */
   float  snr    = 0.0;
   double sumSqX = 0.0, sumSqY = 0.0;
   int    ix;
   float *pRef    = (float *) pX;
   float *pTarget = (float *) pY;
   for (ix = 0; ix < outputSize; ix++) {
      float valRef    = expf((float) pRef[ix]) - logOffset;
      float valTarget = expf((float) pTarget[ix]) - logOffset;
      sumSqX          = sumSqX + ((float) valRef * (float) valRef);
      sumSqY          = sumSqY + ((float) (valRef - (float) valTarget) * ((float) valRef - (float) valTarget));
   }
   if (sumSqX != 0.0f && sumSqY != 0.0f && isfinite(sumSqX) && isfinite(sumSqY)) {
      /* Normal case: both accumulators are finite and non-zero.
       * Standard SNR formula: 10 * log10(signal_power / noise_power) */
      snr = 10.0 * log10(sumSqX / sumSqY);
   }
   return snr;
}
