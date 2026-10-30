// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "../common/TISP_bufDesc.hpp"
#include "../common/TISP_common.hpp"
#include "TISP_twiddle_lut.hpp"
#include <fftlib.h>

namespace TISP {
namespace FFTLIB {

/**
 * @brief   Reports whether a buffer descriptor's rows are packed back to back.
 *
 * @details The FFTLIB batched kernels take 1-D buffer params, so every channel row must start right after the
 *          previous one.
 *
 * @tparam T : Node element type.
 *
 * @param [in] desc : Buffer descriptor to test.
 *
 * @retval true  : @c pitchBytes equals one row of @c width elements.
 * @retval false : The rows are padded.
 * @throw None
 */
template <typename T> bool TISP_isPackedRows(const TISP_BufDesc2D &desc) noexcept
{
   return desc.pitchBytes == (desc.width * sizeof(T));
}

} // namespace FFTLIB
} // namespace TISP
