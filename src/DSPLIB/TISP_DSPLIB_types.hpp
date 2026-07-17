// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef _TISP_DSPLIB_TYPES_HPP_
#define _TISP_DSPLIB_TYPES_HPP_

#include "dsplib.h"
#include <cstdint>

namespace TISP {
namespace DSPLIB {

template <typename T> struct DSPLIB_element_dType {
   static constexpr size_t value = DSPLIB_INT8;
};

template <> struct DSPLIB_element_dType<int8_t> {
   static constexpr size_t value = DSPLIB_INT8;
};

template <> struct DSPLIB_element_dType<int16_t> {
   static constexpr size_t value = DSPLIB_INT16;
};

template <> struct DSPLIB_element_dType<int32_t> {
   static constexpr size_t value = DSPLIB_INT32;
};

template <> struct DSPLIB_element_dType<int64_t> {
   static constexpr size_t value = DSPLIB_INT64;
};

template <> struct DSPLIB_element_dType<float> {
   static constexpr size_t value = DSPLIB_FLOAT32;
};

template <> struct DSPLIB_element_dType<double> {
   static constexpr size_t value = DSPLIB_FLOAT64;
};

template <> struct DSPLIB_element_dType<uint8_t> {
   static constexpr size_t value = DSPLIB_UINT8;
};

template <> struct DSPLIB_element_dType<uint16_t> {
   static constexpr size_t value = DSPLIB_UINT16;
};

template <> struct DSPLIB_element_dType<uint32_t> {
   static constexpr size_t value = DSPLIB_UINT32;
};

template <> struct DSPLIB_element_dType<uint64_t> {
   static constexpr size_t value = DSPLIB_UINT64;
};

} // namespace DSPLIB
} // namespace TISP

#endif
