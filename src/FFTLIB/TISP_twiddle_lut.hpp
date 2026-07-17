// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "../common/TISP_bump_allocator.hpp"
#include <TISP_ErrorCtxt.hpp>
#include <cstdint>
#include <map>
#include <memory>

namespace TISP {
namespace FFTLIB {

/**
 * @brief Shared twiddle-factor lookup table for FFT nodes.
 *
 * Caches twiddle factors, split factors, and related coefficient buffers
 * keyed by FFT size. The first FFT node requesting a given size pays the
 * one-time computation cost; subsequent nodes of the same size receive a
 * @c shared_ptr copy (zero allocation, zero computation).
 *
 * Four independent caches correspond to the four distinct coefficient
 * categories in FFTLIB:
 *
 * | Cache               | Used by             | Generator                   |
 * |---------------------|---------------------|-----------------------------|
 * | Complex twiddles    | FFT1dBatched        | generateComplexTwiddles()   |
 * | Real twiddles       | RFFT / IFFT         | generateRealTwiddles()      |
 * | RFFT split factors  | RFFT1dBatched       | generateRfftSplitFactors()  |
 * | IFFT split factors  | IFFTR1dBatched      | generateIfftSplitFactors()  |
 *
 * RFFT and IFFT share real twiddles (identical @c tw_gen_r2c) but have
 * separate split-factor entries because the cosine sign pattern differs.
 *
 * Uses @c std::shared_ptr<T> (not @c T[]) for C++14 compatibility.
 * The custom deleter returns memory to @c twiddleAllocator() when the
 * last reference is released.
 *
 * @tparam T  Element type (currently only @c float is instantiated).
 */
template <typename T> class TwiddleLUT {
 public:
   /**
    * @brief Get or create complex twiddle factors for FFT1dBatched.
    * @param numPoints  FFT size.
    * @param [in]  errorCtx  Error context for reporting constructor errors
    * @return Shared pointer to twiddle buffer of size @c numPoints*2 elements, or nullptr on error.
    */
   static std::shared_ptr<T> getOrCreateComplexTwiddles(uint32_t numPoints, ErrorCtxt &errorCtx);

   /**
    * @brief Get or create real twiddle factors (shared by RFFT and IFFT).
    * @param numPoints  FFT size.
    * @param [in]  errorCtx  Error context for reporting constructor errors
    * @return Shared pointer to twiddle buffer of size @c numPoints elements, or nullptr on error.
    */
   static std::shared_ptr<T> getOrCreateRealTwiddles(uint32_t numPoints, ErrorCtxt &errorCtx);

   /**
    * @brief Get or create RFFT split factors.
    * @param numPoints  FFT size.
    * @param [in]  errorCtx  Error context for reporting constructor errors
    * @return Shared pointer to split-factor buffer of size @c 2*numPoints elements, or nullptr on error.
    */
   static std::shared_ptr<T> getOrCreateRfftSplitFactors(uint32_t numPoints, ErrorCtxt &errorCtx);

   /**
    * @brief Get or create IFFT split factors.
    * @param numPoints  FFT size.
    * @param [in]  errorCtx  Error context for reporting constructor errors
    * @return Shared pointer to split-factor buffer of size @c 2*numPoints elements, or nullptr on error.
    */
   static std::shared_ptr<T> getOrCreateIfftSplitFactors(uint32_t numPoints, ErrorCtxt &errorCtx);

   /**
    * @brief Total bytes currently cached across all four maps.
    * @return Byte count (element count * sizeof(T) for each entry).
    */
   static size_t totalBytes();

   /**
    * @brief Release all cached entries (shared_ptr refcounts drop; deleters fire
    *        when no external references remain).
    *
    * For same-size workloads (all RFFT/IFFT nodes use one FFT size), clear()
    * fully reclaims arena memory via LIFO deallocation.  For mixed-size
    * workloads, use reset() instead — it unconditionally resets the arena.
    */
   static void clear();

   /**
    * @brief Release all cached entries AND reset the underlying arena.
    *
    * Unlike clear(), which relies on LIFO deallocation order, reset()
    * unconditionally zeros the arena offset.  Use this when tearing down
    * a graph with multiple distinct FFT sizes.
    *
    * @warning All outstanding shared_ptr references to twiddle buffers
    *          become dangling after reset().  Only call this when no
    *          FFT node objects hold twiddle references.
    */
   static void reset();

 private:
   static std::map<uint32_t, std::shared_ptr<T>> _complexTwiddles;
   static std::map<uint32_t, std::shared_ptr<T>> _realTwiddles;
   static std::map<uint32_t, std::shared_ptr<T>> _rfftSplitFactors;
   static std::map<uint32_t, std::shared_ptr<T>> _ifftSplitFactors;
};

} // namespace FFTLIB
} // namespace TISP
