// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*!
 * \file TISP_test.h
 */
#ifndef C71_TISP_TEST_H_
#define C71_TISP_TEST_H_ 1

/*******************************************************************************
 *
 * INCLUDES
 *
 ******************************************************************************/

#include "TI_memory.h" // the local c71 variant
#include "init_mma.h"
#include <time.h> // using the current time to set randomization seed
#ifdef __cplusplus
extern "C" {
#endif
void TISP_TEST_init();
#ifdef __cplusplus
}
#endif
#endif /* C71_TISP_TEST_H_ */
