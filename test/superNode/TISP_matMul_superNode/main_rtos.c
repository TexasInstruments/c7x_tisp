// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#if !defined(HOST_EMULATION)
#include "FreeRTOS.h"
#include "task.h"
#include "ti_board_config.h"
#include "ti_board_open_close.h"
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include <kernel/dpl/DebugP.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#if !defined(HOST_EMULATION)
#define MAIN_TASK_PRI (configMAX_PRIORITIES - 1)

#define MAIN_TASK_SIZE ((65536U) / sizeof(configSTACK_DEPTH_TYPE))
StackType_t gMainTaskStack[MAIN_TASK_SIZE] __attribute__((aligned(16 * 1024)));
__attribute__((section(".l2sramData"), aligned(16 * 1024))) StackType_t gMainTaskStack[MAIN_TASK_SIZE];

StaticTask_t gMainTaskObj;
TaskHandle_t gMainTask;

#endif

#ifdef __cplusplus
extern "C" {
#endif

int test_main_d();

#ifdef __cplusplus
}
#endif

#if !defined(HOST_EMULATION)
void freertos_main(void *args)
{
   int32_t status = SystemP_SUCCESS;

   /* Open drivers */
   Drivers_open();
   /* Open flash and board drivers */
   status = Board_driversOpen();
   DebugP_assert(status == SystemP_SUCCESS);

   test_main_d();

   /* Close board and flash drivers */
   Board_driversClose();
   /* Close drivers */
   Drivers_close();

   vTaskDelete(NULL);
}
#endif

int main()
{
#if !defined(HOST_EMULATION)
   /* init SOC specific modules */
   System_init();
   Board_init();

   /* This task is created at highest priority, it should create more tasks and then delete itself */
   gMainTask =
       xTaskCreateStatic(freertos_main,   /* Pointer to the function that implements the task. */
                         "freertos_main", /* Text name for the task.  This is to facilitate debugging only. */
                         MAIN_TASK_SIZE,  /* Stack depth in units of StackType_t typically uint32_t on 32b CPUs */
                         NULL,            /* We are not using the task parameter. */
                         MAIN_TASK_PRI,   /* task priority, 0 is lowest priority, configMAX_PRIORITIES-1 is highest */
                         gMainTaskStack,  /* pointer to stack base */
                         &gMainTaskObj);  /* pointer to statically allocated task object memory */
   configASSERT(gMainTask != NULL);

   /* Start the scheduler to start the tasks executing. */
   vTaskStartScheduler();

   /* The following line should never be reached because vTaskStartScheduler()
   will only return if there was not enough FreeRTOS heap memory available to
   create the Idle and (if configured) Timer tasks.  Heap management, and
   techniques for trapping heap exhaustion, are described in the book text. */
   DebugP_assertNoLog(0);
#else
   test_main_d();
#endif

   return 0;
}
