// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "TI_profile.h"
#include "TI_test.h"

/* Test parameter to set verbose test feedback level */
uint8_t TI_Verbose_Test_Level = 1;

/* Current test index */
int test_index;

/* Type of kernel under test */
int act_kernel;

int32_t test_cases = 1;

/* Global test description string variables used by all tests */
char desc[512];

/* Global character array used for storing print strings */
char TI_test_print_buffer[512];

char testPatternString[16];

/* Common Test Utility:
 *
 *    TI_print_float
 *
 */
void TI_test_print_float(float num, int printSign)
{
   int digit;
   int i;

   /* Explicitly print sign and remove from num afterwards */
   if (num < 0) {
      num = -num;
      sprintf(TI_test_print_buffer, "- ");
      TI_test_print_string(TI_test_print_buffer);
   }
   else if (printSign) {
      sprintf(TI_test_print_buffer, "+ ");
      TI_test_print_string(TI_test_print_buffer);
   }

   /* Print integer part (always) */
   digit = (int) num;
   num   = (num - (float) digit) * 10;
   sprintf(TI_test_print_buffer, "%d.", digit);
   TI_test_print_string(TI_test_print_buffer);

   /* Print each decimal place until zero remainder */
   for (i = 0; i < ti_PROFILE_SIG_DIGIT; i++) {
      /* Get next digit and display */
      digit = (int) num;
      num   = (num - (float) digit) * 10;
      sprintf(TI_test_print_buffer, "%d", digit);
      TI_test_print_string(TI_test_print_buffer);

      /* Break if remainder is zero */
      if (num == 0) {
         break;
      }
   }
}

/* Common Test Utility:
 *
 *    TI_error_handler
 *
 */
void TI_test_error_handler(void)
{
   /* For now just exit */
   exit(-1);
}

/* Standardized assertion utility */
void TI_test_assert(int assertion, char *err_string)
{
   if (!assertion) {
      /* Display start of error message */
      sprintf(TI_test_print_buffer, "Assertion Error");
      TI_test_print_string(TI_test_print_buffer);

      /* Display test if relavent */
      if (test_index > 0) {
         sprintf(TI_test_print_buffer, " (Test %d)", test_index);
         TI_test_print_string(TI_test_print_buffer);
      }

      /* Display remainder of error message */
      sprintf(TI_test_print_buffer, ": %s\n", err_string);
      TI_test_print_string(TI_test_print_buffer);

      /* Call error handler */
      TI_test_error_handler();
   }
}
