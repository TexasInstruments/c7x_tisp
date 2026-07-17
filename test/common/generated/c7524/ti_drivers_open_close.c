// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */

#include "ti_drivers_open_close.h"
#include <kernel/dpl/DebugP.h>

void Drivers_open(void) { Drivers_uartOpen(); }

void Drivers_close(void) { Drivers_uartClose(); }

/*
 * UART
 */

/* UART Driver handles */
UART_Handle gUartHandle[CONFIG_UART_NUM_INSTANCES];

/* UART Driver Parameters */
UART_Params gUartParams[CONFIG_UART_NUM_INSTANCES] = {
    {
        .baudRate         = 115200,
        .dataLength       = UART_LEN_8,
        .stopBits         = UART_STOPBITS_1,
        .parityType       = UART_PARITY_NONE,
        .readMode         = UART_TRANSFER_MODE_BLOCKING,
        .readReturnMode   = UART_READ_RETURN_MODE_FULL,
        .writeMode        = UART_TRANSFER_MODE_BLOCKING,
        .readCallbackFxn  = NULL,
        .writeCallbackFxn = NULL,
        .hwFlowControl    = FALSE,
        .hwFlowControlThr = UART_RXTRIGLVL_16,
        .transferMode     = UART_CONFIG_MODE_INTERRUPT,
        .skipIntrReg      = FALSE,
        .uartDmaIndex     = -1,
        .intrNum          = 34U,
        .eventId          = 436,
        .intrPriority     = 4U,
        .operMode         = UART_OPER_MODE_16X,
        .rxTrigLvl        = UART_RXTRIGLVL_8,
        .txTrigLvl        = UART_TXTRIGLVL_32,
        .rxEvtNum         = 0U,
        .txEvtNum         = 0U,
    },
};

void Drivers_uartOpen(void)
{
   uint32_t instCnt;
   int32_t  status = SystemP_SUCCESS;

   for (instCnt = 0U; instCnt < CONFIG_UART_NUM_INSTANCES; instCnt++) {
      gUartHandle[instCnt] = NULL; /* Init to NULL so that we can exit gracefully */
   }

   /* Open all instances */
   for (instCnt = 0U; instCnt < CONFIG_UART_NUM_INSTANCES; instCnt++) {
      gUartHandle[instCnt] = UART_open(instCnt, &gUartParams[instCnt]);
      if (NULL == gUartHandle[instCnt]) {
         DebugP_logError("UART open failed for instance %d !!!\r\n", instCnt);
         status = SystemP_FAILURE;
         break;
      }
   }

   if (SystemP_FAILURE == status) {
      Drivers_uartClose(); /* Exit gracefully */
   }

   return;
}

void Drivers_uartClose(void)
{
   uint32_t instCnt;

   /* Close all instances that are open */
   for (instCnt = 0U; instCnt < CONFIG_UART_NUM_INSTANCES; instCnt++) {
      if (gUartHandle[instCnt] != NULL) {
         UART_Config *config;
         config = (UART_Config *) gUartHandle[instCnt];

         if (config->traceInstance != TRUE) {
            UART_close(gUartHandle[instCnt]);
            gUartHandle[instCnt] = NULL;
         }
      }
   }

   return;
}
