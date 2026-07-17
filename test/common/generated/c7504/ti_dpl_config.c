// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */
#include "ti_dpl_config.h"
#include "ti_drivers_config.h"
#include <drivers/soc.h>
#include <kernel/dpl/AddrTranslateP.h>
#include <stdio.h>

/* ----------- DebugP ----------- */
void putchar_(char character)
{
   /* Output to CCS console */
   putchar(character);
   /* Output to UART console */
   DebugP_uartLogWriterPutChar(character);
}

/* ----------- MmuP_armv8 ----------- */
#define CONFIG_MMU_NUM_REGIONS (11u)

MmuP_Config gMmuConfig = {
    .numRegions = CONFIG_MMU_NUM_REGIONS,
    .enableMmu  = 1,
};

MmuP_RegionConfig gMmuRegionConfig[CONFIG_MMU_NUM_REGIONS] =
{
    {
        .vaddr = 0x0u,
        .paddr = 0x0u,
        .size = 0x20000000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR0,
            .global = 1
        }
    },
    {
        .vaddr = 0x20000000u,
        .paddr = 0x20000000u,
        .size = 0x20000000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR0,
            .global = 1
        }
    },
    {
        .vaddr = 0x40000000u,
        .paddr = 0x40000000u,
        .size = 0x20000000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR0,
            .global = 1
        }
    },
    {
        .vaddr = 0x60000000u,
        .paddr = 0x60000000u,
        .size = 0x10000000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR0,
            .global = 1
        }
    },
    {
        .vaddr = 0x7C200000u,
        .paddr = 0x7C200000u,
        .size = 0x100000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR0,
            .global = 1
        }
    },
    {
        .vaddr = 0x7C400000u,
        .paddr = 0x7C400000u,
        .size = 0x100000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR0,
            .global = 1
        }
    },
    {
        .vaddr = 0x80000000u,
        .paddr = 0x80000000u,
        .size = 0x20000000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR7,
            .global = 1
        }
    },
    {
        .vaddr = 0xA0000000u,
        .paddr = 0xA0000000u,
        .size = 0x20000000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR7,
            .global = 1
        }
    },
    {
        .vaddr = 0xAA000000u,
        .paddr = 0xAA000000u,
        .size = 0x2000000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR4,
            .global = 1
        }
    },
    {
        .vaddr = 0x99C10000u,
        .paddr = 0x99C10000u,
        .size = 0x1000u,
        .attr = {
            .accessPerm = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
            .privExecute = 1,
            .userExecute = 0,
            .shareable = MMUP_SHARABLE_OUTER,
            .attrIndx = MMUP_ATTRINDX_MAIR4,
            .global = 1
        }
    },
    {.vaddr = 0x7E000000u,
     .paddr = 0x7E000000u,
     .size  = 0x100000u,
     .attr  = {.accessPerm  = MMUP_ACCESS_PERM_PRIV_RW_USER_NONE,
               .privExecute = 1,
               .userExecute = 0,
               .shareable   = MMUP_SHARABLE_OUTER,
               .attrIndx    = MMUP_ATTRINDX_MAIR7,
               .global      = 1}},
};

/* ----------- ClockP ----------- */
#define TIMER2_CLOCK_SRC_MUX_ADDR (0x1081B8u)
#define TIMER2_CLOCK_SRC_MCU_HFOSC0 (0x0u)
#define TIMER2_BASE_ADDR (0x2420000u)

ClockP_Config gClockConfig = {
    .timerBaseAddr       = TIMER2_BASE_ADDR,
    .timerHwiIntNum      = 10,
    .eventId             = 378,
    .timerInputClkHz     = 25000000,
    .timerInputPreScaler = 1,
    .usecPerTick         = 1000,
};

/* This function is called by __system_start */
void __mmu_init()
{
   MmuP_init();
   CacheP_enable(CacheP_TYPE_ALL);
}

void Dpl_init(void)
{

   /* initialize Hwi but keep interrupts disabled */
   HwiP_init();

   /* init debug log zones early */
   /* Debug log init */
   DebugP_logZoneEnable(DebugP_LOG_ZONE_ERROR);
   DebugP_logZoneEnable(DebugP_LOG_ZONE_WARN);
   /* UART console to use for reading input */
   DebugP_uartSetDrvIndex(CONFIG_UART0);

   /* set timer clock source */
   SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, 2);
   *(volatile uint32_t*) (TIMER2_CLOCK_SRC_MUX_ADDR) = TIMER2_CLOCK_SRC_MCU_HFOSC0;
   SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 2);
   /* initialize Clock */
   ClockP_init();

   /* Enable interrupt handling */
   HwiP_enable();
}

void Dpl_deinit(void)
{
   /* de-initialize Clock */
   ClockP_deinit();
   /* Disable interrupt handling */
   HwiP_disable();
}
