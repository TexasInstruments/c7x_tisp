// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_CORE_COMMON_HPP
#define TISP_CORE_COMMON_HPP

#include <TISP_types.hpp>
#include <TISP_virtualOp.hpp>
#include <cstdint>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/HeapP.h>
#include <memory>
// MISRA Deviation: MISRA.INCL.UNSAFE (Required – MISRA C++ 2023 Rule 17.x)
// Justification: <cstdio> is required for printf/snprintf/vprintf, which are used
// at the C++ / C SDK boundary: (1) the TI DmaUtils logging callback
// (DmaUtilsVprintf_t) is assigned vprintf, as that is the function-pointer
// signature mandated by the C DmaUtils API; (2) error reporting in DMA channel
// configuration and UDMA init failure paths uses printf; (3) AUDIOLIB diagnostic
// output uses snprintf. The C++ <cstdio> header (not the C <stdio.h>) is used,
// placing all symbols in namespace std. No alternative C++ I/O mechanism
// (e.g. std::cout) is available on the C7x bare-metal target where output is
// UART-backed printf only.
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#if defined(HOST_EMULATION)
#include <cstdlib>
#elif __C7X_VEC_SIZE_BITS__ == 512
#include <ti/csl/arch/c7x/cslr_C7X_CPU.h>
#include <ti/csl/csl_clec.h>
#else
#include <drivers/hw_include/csl_clec.h>
#endif

#include "TISP_utils.hpp"

#endif
