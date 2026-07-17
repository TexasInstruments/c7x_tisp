// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_SUPERNODE_COMMONS_HPP
#define TISP_SUPERNODE_COMMONS_HPP

#include "TISP_dmaUtils.hpp"
#include "TISP_superNode.hpp"
// MISRA Deviation: MISRA.INCL.UNSAFE (Required – MISRA C++ 2023 Rule 17.x)
// Justification: <cstdio> is required for printf/snprintf/vprintf used throughout
// the superNode DMA pipeline. Specifically: (1) the TI DmaUtils logging callback
// (DmaUtilsVprintf_t) is assigned vprintf, the function-pointer signature mandated
// by the C DmaUtils API; (2) DMA channel configuration and UDMA init failure paths
// use printf for error reporting. The superNode include chain (TISP_dmaUtils.hpp,
// TISP_superNode.hpp) does not transitively include TISP_common.hpp, so <cstdio>
// must be included here to provide these symbols for all superNode translation units.
// The C++ <cstdio> header (not the C <stdio.h>) is used, placing all symbols in
// namespace std. No alternative C++ I/O mechanism (e.g. std::cout) is available on
// the C7x bare-metal target where output is UART-backed printf only.
#include <cstdio>

#if defined(HOST_EMULATION)
#include <malloc.h>
#endif

#endif
