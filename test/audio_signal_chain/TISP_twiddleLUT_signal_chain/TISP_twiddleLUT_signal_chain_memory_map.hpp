// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include <stdint.h>

#if SOC == j722s || SOC == AM275
#define L2SRAM_SIZE (1024 * 1024) /* 1 MB */
#elif SOC == AM62A || SOC == AM62D
#define L2SRAM_SIZE (800 * 1024) /* 800 KB */
#else
#define L2SRAM_SIZE (800 * 1024) /* default: 800 KB */
#endif

#if defined(HOST_EMULATION)
uint8_t l2Buffer[L2SRAM_SIZE];
#else
__attribute__((section(".l2sramData"), aligned(128))) uint8_t l2Buffer[L2SRAM_SIZE];
#endif
