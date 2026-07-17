// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */
#include "ti_drivers_config.h"
#include <drivers/pinmux.h>

static Pinmux_PerCfg_t gPinMuxMainDomainCfg[] = {

                             /* UART2_RXD -> MCASP0_AXR6 (P18) */
    {
        PIN_MCASP0_AXR6,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE | PIN_DRV_STR_NOMINAL )
    },
                             /* UART2_TXD -> SPI1_CS1 (T4) */
    {
        PIN_SPI1_CS1,
        ( PIN_MODE(3) | PIN_PULL_DISABLE | PIN_DRV_STR_NOMINAL )
    },

    {PINMUX_END, 0U}
};

static Pinmux_PerCfg_t gPinMuxMcuDomainCfg[] = {

    {PINMUX_END, 0U}};

/*
 * Pinmux
 */
void Pinmux_init(void)
{
   Pinmux_config(gPinMuxMainDomainCfg, PINMUX_DOMAIN_ID_MAIN);
   Pinmux_config(gPinMuxMcuDomainCfg, PINMUX_DOMAIN_ID_MCU);
}
