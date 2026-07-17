// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */

#ifndef TI_DRIVERS_CONFIG_H_
#define TI_DRIVERS_CONFIG_H_

#include "ti_dpl_config.h"
#include <drivers/hw_include/cslr_soc.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Common Functions
 */
void System_init(void);
void System_deinit(void);

/*
 * UART
 */
#include <drivers/uart.h>

/* UART Instance Macros */
#define CONFIG_UART0 (0U)
#define CONFIG_UART_NUM_INSTANCES (1U)
#define CONFIG_UART_NUM_DMA_INSTANCES (0U)

#include <drivers/soc.h>
#include <kernel/dpl/CycleCounterP.h>

#ifdef __cplusplus
}
#endif

#endif /* TI_DRIVERS_CONFIG_H_ */
