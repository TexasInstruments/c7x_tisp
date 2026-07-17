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

namespace TISP {

/**
 * @brief Generic 2-D buffer descriptor: pointer, dimensions, and row stride.
 *
 */
typedef struct TISP_BufDesc2D_t {
   void    *pBuf;     /*!< Buffer pointer. */
   uint32_t width;    /*!< dim_x in elements. */
   uint32_t height;   /*!< dim_y in elements. */
   int32_t  stride_y; /*!< Row stride in bytes. */
} TISP_BufDesc2D;

} // namespace TISP
