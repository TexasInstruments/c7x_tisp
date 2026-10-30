// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// Arena configuration for the TwiddleLUT signal-chain test.
// Replaces test/common/TISP_app_arena_default_config.cpp for this binary: up to four Gain -> RFFT -> IFFT subgraphs
// live at once, which needs a 32KB handle arena on every device.

#include <cstddef>
#include <cstdint>

// Node arena — 32KB (128-byte aligned)
#if !defined(HOST_EMULATION)
__attribute__((section(".tisp_node_arena"), aligned(128)))
#else
__attribute__((aligned(128)))
#endif
uint8_t tisp_nodeArenaBuffer[32U * 1024U];
extern const size_t tisp_nodeArenaSize = sizeof(tisp_nodeArenaBuffer);

// Twiddle arena — 32KB (64-byte aligned)
#if !defined(HOST_EMULATION)
__attribute__((section(".tisp_twiddle_arena"), aligned(64)))
#else
__attribute__((aligned(64)))
#endif
uint8_t tisp_twiddleArenaBuffer[32U * 1024U];
extern const size_t tisp_twiddleArenaSize = sizeof(tisp_twiddleArenaBuffer);

// Handle arena — 32KB (32-byte aligned)
#if !defined(HOST_EMULATION)
__attribute__((section(".tisp_handle_arena"), aligned(32)))
#else
__attribute__((aligned(32)))
#endif
uint8_t tisp_handleArenaBuffer[32U * 1024U];
extern const size_t tisp_handleArenaSize = sizeof(tisp_handleArenaBuffer);
