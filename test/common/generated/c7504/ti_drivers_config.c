// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */

#include "ti_drivers_config.h"
#include <drivers/sciclient.h>

/*
 * UART
 */

/* UART atrributes */
static UART_Attrs gUartAttrs[CONFIG_UART_NUM_INSTANCES] = {
    {
        .baseAddr     = CSL_MCU_UART0_BASE,
        .inputClkFreq = 48000000U,
    },
};
/* UART objects - initialized by the driver */
static UART_Object gUartObjects[CONFIG_UART_NUM_INSTANCES];
/* UART driver configuration */
UART_Config gUartConfig[CONFIG_UART_NUM_INSTANCES] = {
    {
        .attrs         = &gUartAttrs[CONFIG_UART0],
        .object        = &gUartObjects[CONFIG_UART0],
        .traceInstance = FALSE,
    },
};

uint32_t gUartConfigNum = CONFIG_UART_NUM_INSTANCES;

#include <drivers/uart/v0/dma/uart_dma.h>

UART_DmaConfig gUartDmaConfig[CONFIG_UART_NUM_DMA_INSTANCES] = {};

uint32_t gUartDmaConfigNum = CONFIG_UART_NUM_DMA_INSTANCES;

void Drivers_uartInit(void) { UART_init(); }

void Pinmux_init();
void PowerClock_init(void);
void PowerClock_deinit(void);

/*
 * Common Functions
 */
void System_init(void)
{
   /* DPL init sets up address transalation unit, on some CPUs this is needed
    * to access SCICLIENT services, hence this needs to happen first
    */
   Dpl_init();
   /* We should do sciclient init before we enable power and clock to the peripherals */
   /* SCICLIENT init */
   {

      int32_t retVal = SystemP_SUCCESS;

      retVal = Sciclient_init(CSL_CORE_ID_C75SS0_0);
      DebugP_assertNoLog(SystemP_SUCCESS == retVal);
   }

   PowerClock_init();
   /* Now we can do pinmux */
   Pinmux_init();
   /* finally we initialize all peripheral drivers */
   Drivers_uartInit();
}

void System_deinit(void)
{
   UART_deinit();
   PowerClock_deinit();
   /* SCICLIENT deinit */
   {
      int32_t retVal = SystemP_SUCCESS;

      retVal = Sciclient_deinit();
      DebugP_assertNoLog(SystemP_SUCCESS == retVal);
   }
   Dpl_deinit();
}
