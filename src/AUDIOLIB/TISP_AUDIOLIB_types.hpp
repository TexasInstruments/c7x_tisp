// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef _TISP_AUDIOLIB_TYPES_HPP_
#define _TISP_AUDIOLIB_TYPES_HPP_

#include "audiolib.h"
#include <cstdint>

namespace TISP {
namespace AUDIOLIB {

template <typename T> struct AUDIOLIB_element_dType {
   static constexpr size_t value = AUDIOLIB_INT8;
};

template <> struct AUDIOLIB_element_dType<int8_t> {
   static constexpr size_t value = AUDIOLIB_INT8;
};

template <> struct AUDIOLIB_element_dType<int16_t> {
   static constexpr size_t value = AUDIOLIB_INT16;
};

template <> struct AUDIOLIB_element_dType<int32_t> {
   static constexpr size_t value = AUDIOLIB_INT32;
};

template <> struct AUDIOLIB_element_dType<int64_t> {
   static constexpr size_t value = AUDIOLIB_INT64;
};

template <> struct AUDIOLIB_element_dType<float> {
   static constexpr size_t value = AUDIOLIB_FLOAT32;
};

template <> struct AUDIOLIB_element_dType<double> {
   static constexpr size_t value = AUDIOLIB_FLOAT64;
};

template <> struct AUDIOLIB_element_dType<uint8_t> {
   static constexpr size_t value = AUDIOLIB_UINT8;
};

template <> struct AUDIOLIB_element_dType<uint16_t> {
   static constexpr size_t value = AUDIOLIB_UINT16;
};

template <> struct AUDIOLIB_element_dType<uint32_t> {
   static constexpr size_t value = AUDIOLIB_UINT32;
};

template <> struct AUDIOLIB_element_dType<uint64_t> {
   static constexpr size_t value = AUDIOLIB_UINT64;
};

} // namespace AUDIOLIB
} // namespace TISP

#endif
