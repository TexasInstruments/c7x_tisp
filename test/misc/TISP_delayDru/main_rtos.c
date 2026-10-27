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

   Drivers_open();
   status = Board_driversOpen();
   DebugP_assert(status == SystemP_SUCCESS);

   test_main_d();

   Board_driversClose();
   Drivers_close();

   vTaskDelete(NULL);
}
#endif

int main()
{
#if !defined(HOST_EMULATION)
   System_init();
   Board_init();

   gMainTask =
       xTaskCreateStatic(freertos_main, "freertos_main", MAIN_TASK_SIZE,
                         NULL, MAIN_TASK_PRI, gMainTaskStack, &gMainTaskObj);
   configASSERT(gMainTask != NULL);

   vTaskStartScheduler();
   DebugP_assertNoLog(0);
#else
   test_main_d();
#endif

   return 0;
}
