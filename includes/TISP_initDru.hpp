// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include <cstdint>
#include <cstdio>

void App_dmautilsClecInitDru();

namespace TISP {
void appC7xClecInitDru(void);

int32_t test_sciclientDmscGetVersion(char *version_str, uint32_t version_str_size);

} // namespace TISP
