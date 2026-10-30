// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_DSPLIB_COMMONS_HPP
#define TISP_DSPLIB_COMMONS_HPP

#include "../common/TISP_bufDesc.hpp"
#include "../common/TISP_common.hpp"
#include "TISP_DSPLIB_types.hpp"
#include <dsplib.h>

namespace TISP {
namespace DSPLIB {

/**
 * @brief   Converts a TISP buffer descriptor into the DSPLIB 2-D buffer params of element type @c T.
 *
 * @tparam T : Node element type.
 *
 * @param [in] desc : Validated TISP buffer descriptor (see @ref TISP_checkBufDescList).
 *
 * @retval DSPLIB_bufParams2D_t : DSPLIB buffer params with the same type, dimensions, and pitch.
 * @throw None
 */
template <typename T> DSPLIB_bufParams2D_t TISP_toDsplibBufParams2D(const TISP_BufDesc2D &desc) noexcept
{
   DSPLIB_bufParams2D_t bufParams;
   bufParams.data_type = static_cast<uint32_t>(DSPLIB_element_dType<T>::value);
   bufParams.dim_x     = static_cast<uint32_t>(desc.width);
   bufParams.dim_y     = static_cast<uint32_t>(desc.height);
   bufParams.stride_y  = static_cast<int32_t>(desc.pitchBytes);
   return bufParams;
}

} // namespace DSPLIB
} // namespace TISP

#endif
