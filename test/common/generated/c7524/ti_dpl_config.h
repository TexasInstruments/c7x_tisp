// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*
 * Auto generated file
 */

#ifndef TI_DPL_CONFIG_H
#define TI_DPL_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/dpl/HwiP.h>

#include <kernel/dpl/DebugP.h>

#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/MmuP_armv8.h>

void Dpl_init(void);
void Dpl_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* TI_DPL_CONFIG_H */
