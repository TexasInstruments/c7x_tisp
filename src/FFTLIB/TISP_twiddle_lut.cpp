// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_twiddle_lut.hpp"
#include "TISP_ErrorCtxt.hpp"
#include "TISP_FFTLIB_common.hpp"

#include <cmath>
#include <cstdint>

namespace TISP {

// ---------------------------------------------------------------------------
// Twiddle / split-factor generation (extracted from per-node member functions)
// ---------------------------------------------------------------------------

/**
 * @brief Generate radix-4 complex twiddle factors.
 *
 * Extracted from FFT1dBatched::tw_gen(). Computes W(k,N) = cos(2*pi*k/N) - j*sin(2*pi*k/N)
 * for three twiddle sets per radix-4 stage.
 *
 * @param pW         Output buffer (numPoints * 2 elements).
 * @param numPoints  FFT size.
 */
static void generateComplexTwiddles(float *pW, uint32_t numPoints)
{
   uint32_t     i;
   uint32_t     j;
   uint32_t     k;
   uint32_t     t;
   const double PI = 3.141592654;
   t               = numPoints >> 2U;
   j               = 1U;
   k               = 0U;
   while (j <= (numPoints >> 2U)) {
      for (i = 0U; i < (numPoints >> 2U); i += j) {
         pW[k]      = static_cast<FFTLIB_F32>(cos(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints)));
         pW[k + 1U] = static_cast<FFTLIB_F32>(-sin(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints)));
         pW[k + 2U * t] =
             static_cast<FFTLIB_F32>(cos(4.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints)));
         pW[k + 2U * t + 1U] =
             static_cast<FFTLIB_F32>(-sin(4.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints)));
         pW[k + 4U * t] =
             static_cast<FFTLIB_F32>(cos(6.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints)));
         pW[k + 4U * t + 1U] =
             static_cast<FFTLIB_F32>(-sin(6.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints)));
         k += 2U;
      }
      k += t * 4U;
      t >>= 2U;
      j <<= 2U;
   }
}

/**
 * @brief Generate real twiddle factors (half-size complex FFT).
 *
 * Extracted from RFFT1dBatched::tw_gen_r2c() (identical in IFFT).
 * Operates on numPoints/2 (half-size FFT for real input).
 *
 * @param pW         Output buffer (numPoints elements).
 * @param numPoints  FFT size.
 */
static void generateRealTwiddles(float *pW, uint32_t numPoints)
{
   uint32_t     i;
   uint32_t     j;
   uint32_t     k;
   uint32_t     t;
   const double PI = 3.141592654;

   uint32_t numPointsLocal = numPoints >> 1U;

   t = numPointsLocal >> 2U;
   j = 1U;
   k = 0U;
   while (j <= (numPointsLocal >> 2U)) {
      for (i = 0U; i < (numPointsLocal >> 2U); i += j) {
         pW[k] = static_cast<FFTLIB_F32>(cos(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPointsLocal)));
         pW[k + 1U] =
             static_cast<FFTLIB_F32>(-sin(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPointsLocal)));
         pW[k + 2U * t] =
             static_cast<FFTLIB_F32>(cos(4.0 * PI * static_cast<double>(i) / static_cast<double>(numPointsLocal)));
         pW[k + 2U * t + 1U] =
             static_cast<FFTLIB_F32>(-sin(4.0 * PI * static_cast<double>(i) / static_cast<double>(numPointsLocal)));
         pW[k + 4U * t] =
             static_cast<FFTLIB_F32>(cos(6.0 * PI * static_cast<double>(i) / static_cast<double>(numPointsLocal)));
         pW[k + 4U * t + 1U] =
             static_cast<FFTLIB_F32>(-sin(6.0 * PI * static_cast<double>(i) / static_cast<double>(numPointsLocal)));
         k += 2U;
      }
      k += t * 4U;
      t >>= 2U;
      j <<= 2U;
   }
}

/**
 * @brief Generate RFFT split factors (mirror indexing, -cos/+cos).
 *
 * Extracted from RFFT1dBatched::split_factor_gen_r2c().
 *
 * @param pSf        Output buffer (2 * numPoints elements).
 * @param numPoints  FFT size.
 */
static void generateRfftSplitFactors(float *pSf, uint32_t numPoints)
{
   uint32_t     i;
   const double PI = 3.141592654;

   for (i = 0U; i < (numPoints >> 1U); i++) {
      pSf[2U * i] = static_cast<FFTLIB_F32>(
          0.5 * (1.0 - sin(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints))));
      pSf[2U * i + 1U] =
          static_cast<FFTLIB_F32>(0.5 * (-cos(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints))));

      pSf[2U * numPoints - 2U * i - 2U] = static_cast<FFTLIB_F32>(
          0.5 * (1.0 + sin(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints))));
      pSf[2U * numPoints - 2U * i - 2U + 1U] =
          static_cast<FFTLIB_F32>(0.5 * (cos(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints))));
   }
}

/**
 * @brief Generate IFFT split factors (sequential offset, +cos/-cos).
 *
 * Extracted from IFFTR1dBatched::split_factor_gen_r2c().
 *
 * @param pSf        Output buffer (2 * numPoints elements).
 * @param numPoints  FFT size.
 */
static void generateIfftSplitFactors(float *pSf, uint32_t numPoints)
{
   uint32_t     i;
   const double PI = 3.141592654;

   for (i = 0U; i < (numPoints >> 1U); i++) {
      pSf[2U * i] = static_cast<FFTLIB_F32>(
          0.5 * (1.0 - sin(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints))));
      pSf[2U * i + 1U] =
          static_cast<FFTLIB_F32>(0.5 * (cos(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints))));

      pSf[2U * i + numPoints] = static_cast<FFTLIB_F32>(
          0.5 * (1.0 + sin(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints))));
      pSf[2U * i + numPoints + 1U] =
          static_cast<FFTLIB_F32>(0.5 * (-cos(2.0 * PI * static_cast<double>(i) / static_cast<double>(numPoints))));
   }
}

// ---------------------------------------------------------------------------
// Helper: allocate from twiddleAllocator, wrap in shared_ptr with custom deleter
// ---------------------------------------------------------------------------

static std::shared_ptr<float> allocateShared(size_t count, ErrorCtxt &errorCtx)
{
   std::shared_ptr<float> x = nullptr;
   if (count == 0) {
      errorCtx.setInvalidParameterError("twiddleLUT constructor", "allocateShared :: count must be greater than zero");
   }

   if (errorCtx.isSuccess()) {
      TwiddlePtr<float> owned = twiddleAllocator().allocateCustomAligned<float>(count, kTwiddleAlignment);
      if (owned == nullptr) {
         errorCtx.setMemoryError("twiddleLUT constructor",
                                 "allocateShared :: failed to allocate twiddle memory from arena");
      }

      if (errorCtx.isSuccess()) {
         x = std::shared_ptr<float>(owned.release(), [](float *p) { twiddleAllocator().deallocate(p); });
      }
   }
   return x;
}

// ---------------------------------------------------------------------------
// Static map definitions — TwiddleLUT<float>
// ---------------------------------------------------------------------------

namespace FFTLIB {

template <> std::map<uint32_t, std::shared_ptr<float>> TwiddleLUT<float>::_complexTwiddles{};
template <> std::map<uint32_t, std::shared_ptr<float>> TwiddleLUT<float>::_realTwiddles{};
template <> std::map<uint32_t, std::shared_ptr<float>> TwiddleLUT<float>::_rfftSplitFactors{};
template <> std::map<uint32_t, std::shared_ptr<float>> TwiddleLUT<float>::_ifftSplitFactors{};

// ---------------------------------------------------------------------------
// TwiddleLUT<float> — getOrCreate methods
//
// Each method uses a guard-clause pattern (early returns) rather than nested
// if-else to keep the happy path flat and readable. Two guards:
//   1. LUT hit    — return existing shared_ptr (zero allocation, refcount++)
//   2. OOM        — arena full, return nullptr so caller can detect failure
// The happy path (generate + store + return) runs only on a LUT miss with
// sufficient arena space. This avoids a status-flag variable (bool ok) and
// nested if-else chains that grow with each precondition. MISRA C++:2023
// permits multiple return statements (the single-exit rule from MISRA
// C++:2008 Rule 6-6-5 was dropped).
// ---------------------------------------------------------------------------

template <>
std::shared_ptr<float> TwiddleLUT<float>::getOrCreateComplexTwiddles(uint32_t numPoints, ErrorCtxt &errorCtx)
{
   std::shared_ptr<float> shared = nullptr;
   if (numPoints == 0) {
      errorCtx.setInvalidParameterError("twiddleLUT constructor",
                                        "getOrCreateComplexTwiddles :: numPoints must be greater than zero");
   }

   if (errorCtx.isSuccess()) {
      auto it = _complexTwiddles.find(numPoints); // Guard 1: LUT hit
      if (it != _complexTwiddles.end()) {
         shared = it->second;
      }
      else {
         shared = allocateShared(numPoints * 2U, errorCtx);
         if (shared == nullptr) {
            errorCtx.setNullPointerError(
                "twiddleLUT constructor",
                "getOrCreateComplexTwiddles :: shared pointer is null"); // Guard 2: arena OOM or error
         }
         if (errorCtx.isSuccess()) {
            generateComplexTwiddles(shared.get(), numPoints);
            _complexTwiddles[numPoints] = shared;
         }
      }
   }
   return shared;
}

template <> std::shared_ptr<float> TwiddleLUT<float>::getOrCreateRealTwiddles(uint32_t numPoints, ErrorCtxt &errorCtx)
{
   std::shared_ptr<float> shared = nullptr;
   if (numPoints == 0) {
      errorCtx.setInvalidParameterError("twiddleLUT constructor",
                                        "getOrCreateRealTwiddles :: numPoints must be greater than zero");
   }

   if (errorCtx.isSuccess()) {
      auto it = _realTwiddles.find(numPoints); // Guard 1: LUT hit
      if (it != _realTwiddles.end()) {
         shared = it->second;
      }
      else {
         shared = allocateShared(numPoints, errorCtx);
         if (shared == nullptr) {
            errorCtx.setNullPointerError(
                "twiddleLUT constructor",
                "getOrCreateRealTwiddles :: shared pointer is null"); // Guard 2: arena OOM or error
         }
         if (errorCtx.isSuccess()) {
            generateRealTwiddles(shared.get(), numPoints);
            _realTwiddles[numPoints] = shared;
         }
      }
   }
   return shared;
}

template <>
std::shared_ptr<float> TwiddleLUT<float>::getOrCreateRfftSplitFactors(uint32_t numPoints, ErrorCtxt &errorCtx)
{
   std::shared_ptr<float> shared = nullptr;
   if (numPoints == 0) {
      errorCtx.setInvalidParameterError("twiddleLUT constructor",
                                        "getOrCreateRfftSplitFactors :: numPoints must be greater than zero");
   }
   if (errorCtx.isSuccess()) {
      auto it = _rfftSplitFactors.find(numPoints); // Guard 1: LUT hit
      if (it != _rfftSplitFactors.end()) {
         shared = it->second;
      }
      else {
         shared = allocateShared(2U * numPoints, errorCtx);
         if (shared == nullptr) {
            errorCtx.setNullPointerError(
                "twiddleLUT constructor",
                "getOrCreateRfftSplitFactors :: shared pointer is null"); // Guard 2: arena OOM or error
         }
         if (errorCtx.isSuccess()) {
            generateRfftSplitFactors(shared.get(), numPoints);
            _rfftSplitFactors[numPoints] = shared;
         }
      }
   }
   return shared;
}

template <>
std::shared_ptr<float> TwiddleLUT<float>::getOrCreateIfftSplitFactors(uint32_t numPoints, ErrorCtxt &errorCtx)
{
   std::shared_ptr<float> shared = nullptr;
   if (numPoints == 0) {
      errorCtx.setInvalidParameterError("twiddleLUT constructor",
                                        "getOrCreateIfftSplitFactors :: numPoints must be greater than zero");
   }
   if (errorCtx.isSuccess()) {
      auto it = _ifftSplitFactors.find(numPoints); // Guard 1: LUT hit
      if (it != _ifftSplitFactors.end()) {
         shared = it->second;
      }
      else {
         shared = allocateShared(2U * numPoints, errorCtx);
         if (shared == nullptr) {
            errorCtx.setNullPointerError(
                "twiddleLUT constructor",
                "getOrCreateIfftSplitFactors :: shared pointer is null"); // Guard 2: arena OOM or error
         }
         if (errorCtx.isSuccess()) {
            generateIfftSplitFactors(shared.get(), numPoints);
            _ifftSplitFactors[numPoints] = shared;
         }
      }
   }
   return shared;
}

// ---------------------------------------------------------------------------
// TwiddleLUT<float> — totalBytes / clear / reset
// ---------------------------------------------------------------------------

template <> size_t TwiddleLUT<float>::totalBytes()
{
   size_t bytes = 0U;
   for (const auto &entry : _complexTwiddles) {
      bytes += static_cast<size_t>(entry.first) * 2U * sizeof(float); // numPoints * 2 elements
   }
   for (const auto &entry : _realTwiddles) {
      bytes += static_cast<size_t>(entry.first) * sizeof(float); // numPoints elements
   }
   for (const auto &entry : _rfftSplitFactors) {
      bytes += 2U * static_cast<size_t>(entry.first) * sizeof(float); // 2 * numPoints elements
   }
   for (const auto &entry : _ifftSplitFactors) {
      bytes += 2U * static_cast<size_t>(entry.first) * sizeof(float); // 2 * numPoints elements
   }
   return bytes;
}

template <> void TwiddleLUT<float>::clear()
{
   // LIFO order: deallocate last-allocated first so the bump allocator
   // can reclaim each block (it only reclaims the top-of-stack block).
   _ifftSplitFactors.clear();
   _rfftSplitFactors.clear();
   _realTwiddles.clear();
   _complexTwiddles.clear();
}

template <> void TwiddleLUT<float>::reset()
{
   // Drop all shared_ptr references first (deleters call deallocate,
   // but we don't rely on that — the arena reset below is unconditional).
   _ifftSplitFactors.clear();
   _rfftSplitFactors.clear();
   _realTwiddles.clear();
   _complexTwiddles.clear();

   // Unconditionally zero the arena offset — handles mixed-size workloads
   // where interleaved allocations prevent LIFO reclamation.
   twiddleAllocator().reset();
}

} // namespace FFTLIB
} // namespace TISP
