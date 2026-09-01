// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include <cstdint>

namespace TISP {

/* typedef std::vector<VirtualOp *> opVec; */

/* typedef std::list<VirtualOp *> opList; */
/* class dmaChOffset { */
/*  public: */
/*    static size_t globalChOffset; */
/* }; */

/* typedef std::vector<TISP::SuperNode::superNode *> superNodeVec; */
/* typedef enum { NO_DMA = 0, DMA_IN, DMA_OUT, DMA_IN_OUT } dmaDir_t; */
enum class Types : uint32_t {
    kInt8   = 0,  /**< Signed 8-bit integer */
    kInt16  = 1,  /**< Signed 16-bit integer */
    kInt32  = 2,  /**< Signed 32-bit integer */
    kInt64  = 3,  /**< Signed 64-bit integer */
    kInt128 = 4,  /**< Signed 128-bit integer */

    kUint8   = 5,  /**< Unsigned 8-bit integer */
    kUint16  = 6,  /**< Unsigned 16-bit integer */
    kUint24  = 7,  /**< Unsigned 24-bit integer */
    kUint32  = 8,  /**< Unsigned 32-bit integer */
    kUint64  = 9,  /**< Unsigned 64-bit integer */
    kUint128 = 10, /**< Unsigned 128-bit integer */

    /* Standard floating point data types */
    kFloat16 = 11, /**< 16-bit floating point number */
    kFloat32 = 12, /**< 32-bit floating point number */
    kFloat64 = 13  /**< 64-bit floating point number */
};

/**
 * @brief   Returns the size in bytes of a single @ref TISP::Types element.
 * @details Usable in constant expressions (e.g. buffer-stride math).
 * @param [in] type : Data-type enumerator to size.
 * @retval Number of bytes per element (1 for an unrecognized enumerator).
 */
constexpr uint32_t sizeOf(Types type)
{
   return ((type == Types::kInt8) || (type == Types::kUint8))                                    ? 1U
        : ((type == Types::kInt16) || (type == Types::kUint16) || (type == Types::kFloat16))     ? 2U
        : (type == Types::kUint24)                                                               ? 3U
        : ((type == Types::kInt32) || (type == Types::kUint32) || (type == Types::kFloat32))     ? 4U
        : ((type == Types::kInt64) || (type == Types::kUint64) || (type == Types::kFloat64))     ? 8U
        : ((type == Types::kInt128) || (type == Types::kUint128))                                ? 16U
                                                                                                 : 1U;
}

/**
 * @struct ElementDType
 * @brief Maps a C++ element type @c T to its corresponding @ref TISP::Types enumerator.
 * @tparam T : C++ element type to map.
 *
 * @details Type-trait used by nodes to populate the data-type fields of
 *          @ref NodeBufParams from the node's template parameter, without
 *          casting library-specific enumerators. The primary template defaults
 *          to @ref Types::kInt8; each supported type provides an explicit
 *          specialization below.
 */
template <typename T> struct ElementDType {
   static constexpr Types value = Types::kInt8; /**< @brief Resolved @ref TISP::Types enumerator for @c T. */
};

template <> struct ElementDType<int8_t> {
   static constexpr Types value = Types::kInt8;
};

template <> struct ElementDType<int16_t> {
   static constexpr Types value = Types::kInt16;
};

template <> struct ElementDType<int32_t> {
   static constexpr Types value = Types::kInt32;
};

template <> struct ElementDType<int64_t> {
   static constexpr Types value = Types::kInt64;
};

template <> struct ElementDType<uint8_t> {
   static constexpr Types value = Types::kUint8;
};

template <> struct ElementDType<uint16_t> {
   static constexpr Types value = Types::kUint16;
};

template <> struct ElementDType<uint32_t> {
   static constexpr Types value = Types::kUint32;
};

template <> struct ElementDType<uint64_t> {
   static constexpr Types value = Types::kUint64;
};

template <> struct ElementDType<float> {
   static constexpr Types value = Types::kFloat32;
};

template <> struct ElementDType<double> {
   static constexpr Types value = Types::kFloat64;
};

} // namespace TISP
