// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */

#ifndef TI_DRVIERS_OPEN_CLOSE_H_
#define TI_DRVIERS_OPEN_CLOSE_H_

#include "ti_drivers_config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Common Functions
 */
void Drivers_open(void);
void Drivers_close(void);

/*
 * UART
 */
#include <drivers/uart.h>

/* UART Driver handles */
extern UART_Handle gUartHandle[CONFIG_UART_NUM_INSTANCES];

/*
 * UART Driver Advance Parameters - to be used only when Driver_open() and
 * Driver_close() is not used by the application
 */
/* UART Driver Parameters */
extern UART_Params gUartParams[CONFIG_UART_NUM_INSTANCES];
/* UART Driver open/close - can be used by application when Driver_open() and
 * Driver_close() is not used directly and app wants to control the various driver
 * open/close sequences */
void Drivers_uartOpen(void);
void Drivers_uartClose(void);

#ifdef __cplusplus
}
#endif

#endif /* TI_DRVIERS_OPEN_CLOSE_H_ */
