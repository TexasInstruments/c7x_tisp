// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */
#include <drivers/soc.h>
#include <kernel/dpl/DebugP.h>

#define SOC_MODULES_END (0xFFFFFFFFu)

typedef struct {

   uint32_t moduleId;
   uint32_t clkId;
   uint32_t clkRate;

} SOC_ModuleClockFrequency;

uint32_t gSocModules[] = {
    TISCI_DEV_UART2,

    SOC_MODULES_END,
};

SOC_ModuleClockFrequency gSocModulesClockFrequency[] = {
    {TISCI_DEV_UART2, TISCI_DEV_UART2_FCLK_CLK, 48000000},

    {SOC_MODULES_END, SOC_MODULES_END, SOC_MODULES_END},
};

void Module_clockEnable(void)
{
   int32_t  status;
   uint32_t i = 0;

   while (gSocModules[i] != SOC_MODULES_END) {
      status = SOC_moduleClockEnable(gSocModules[i], 1);
      DebugP_assertNoLog(status == SystemP_SUCCESS);
      i++;
   }
}

void Module_clockDisable(void)
{
   int32_t  status;
   uint32_t i = 0;

   while (gSocModules[i] != SOC_MODULES_END) {
      status = SOC_moduleClockEnable(gSocModules[i], 0);
      DebugP_assertNoLog(status == SystemP_SUCCESS);
      i++;
   }
}

void Module_clockSetFrequency(void)
{
   int32_t  status;
   uint32_t i = 0;

   while (gSocModulesClockFrequency[i].moduleId != SOC_MODULES_END) {
      status = SOC_moduleSetClockFrequency(gSocModulesClockFrequency[i].moduleId, gSocModulesClockFrequency[i].clkId,
                                           gSocModulesClockFrequency[i].clkRate);
      DebugP_assertNoLog(status == SystemP_SUCCESS);
      i++;
   }
}

void PowerClock_init(void)
{
   Module_clockEnable();
   Module_clockSetFrequency();
}

void PowerClock_deinit() { Module_clockDisable(); }
