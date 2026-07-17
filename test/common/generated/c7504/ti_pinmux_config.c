// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */
#include "ti_drivers_config.h"
#include <drivers/pinmux.h>

static Pinmux_PerCfg_t gPinMuxMainDomainCfg[] = {


    {PINMUX_END, 0U}
};

static Pinmux_PerCfg_t gPinMuxMcuDomainCfg[] = {

    /* MCU_USART0 pin config */
            /* MCU_UART0_RXD -> MCU_UART0_RXD (D8) */
    {
        PIN_MCU_UART0_RXD,
        ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE | PIN_DRV_STR_NOMINAL )
    },
            /* MCU_UART0_TXD -> MCU_UART0_TXD (F8) */
    {
        PIN_MCU_UART0_TXD,
        ( PIN_MODE(0) | PIN_PULL_DISABLE | PIN_DRV_STR_NOMINAL )
    },

    {PINMUX_END, 0U}
};

/*
 * Pinmux
 */
void Pinmux_init(void)
{
    SOC_fixFastDriveStrength();
    Pinmux_config(gPinMuxMainDomainCfg, PINMUX_DOMAIN_ID_MAIN);
    Pinmux_config(gPinMuxMcuDomainCfg, PINMUX_DOMAIN_ID_MCU);
}

