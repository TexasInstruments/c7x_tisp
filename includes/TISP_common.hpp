// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_types.hpp"
#include "TISP_virtualOp.hpp"
#include <c7x_scalable.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// TISP_BufDesc2D / TISP_BufDescList are defined in TISP_types.hpp.

/*
 * Byte alignment for SE/SA streaming scratch sub-buffers.  64 bytes is the maximum
 * C7x vector width (512-bit cores) and the cache-line / 64-byte-aligned allocation
 * boundary node scratch is placed on; over-aligning to 64 B is safe on 256-bit cores
 * too.  Nodes round scratch regions up to this boundary (TISP_SE_SA_ALIGN_BYTES /
 * sizeof(T) elements, a power of two for float/double) so SE/SA base pointers stay
 * vector-aligned.  Used by Stft / Istft (calcScratchSize must match the constructor).
 */
#ifndef TISP_SE_SA_ALIGN_BYTES
#define TISP_SE_SA_ALIGN_BYTES (64U)
#endif
