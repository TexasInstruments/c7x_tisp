// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "../common/TISP_bufDesc.hpp"
#include "../common/TISP_common.hpp"
#include "TISP_AUDIOLIB_types.hpp"
#include <audiolib.h>

namespace TISP {
namespace AUDIOLIB {

/**
 * @brief   Converts a TISP buffer descriptor into the AUDIOLIB 2-D buffer params of element type @c T.
 *
 * @tparam T : Node element type.
 *
 * @param [in] desc : Validated TISP buffer descriptor (see @ref TISP_checkBufDescList).
 *
 * @retval AUDIOLIB_bufParams2D_t : AUDIOLIB buffer params with the same type, dimensions, and pitch.
 * @throw None
 */
template <typename T> AUDIOLIB_bufParams2D_t TISP_toAudiolibBufParams2D(const TISP_BufDesc2D &desc) noexcept
{
   AUDIOLIB_bufParams2D_t bufParams;
   bufParams.data_type = static_cast<uint32_t>(AUDIOLIB_element_dType<T>::value);
   bufParams.dim_x     = static_cast<uint32_t>(desc.width);
   bufParams.dim_y     = static_cast<uint32_t>(desc.height);
   bufParams.stride_y  = static_cast<int32_t>(desc.pitchBytes);
   return bufParams;
}

} // namespace AUDIOLIB
} // namespace TISP
