// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

// ---------------------------------------------------------------------------
// Compatibility shim — forwards to the promoted library allocator.
// The implementation now lives in src/common/TISP_bump_allocator.{hpp,cpp}.
// This header preserves the original names so that 100+ existing test files
// continue to compile without modification.
// ---------------------------------------------------------------------------

#include "../../src/common/TISP_bump_allocator.hpp"

using UnitTestHeap        = TISP::BumpAllocator;
using UnitTestHeapDeleter = TISP::BumpAllocatorDeleter;
