// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include <cmath>
#include <cstdint>

namespace TISP {

/**
 * @brief Window function type selector.
 *
 * @details Identifies the window function to generate. All cosine-sum
 *          windows (Hann, Hamming, Blackman, Blackman-Harris, sqrt(Hann))
 *          support both periodic and symmetric modes. Bartlett is always
 *          symmetric.
 */
enum class WindowType : uint32_t {
   kRectangular = 0,
   kHann,
   kHamming,
   kBlackman,
   kBlackmanHarris,
   kBartlett,
   kSqrtHann
};

/**
 * @brief Window placement within a larger (zero-padded) buffer.
 *
 * @details Controls where a \p windowLength window is positioned inside a
 *          \p bufferSize-long buffer when \p windowLength < \p bufferSize:
 *          - \c kCenter : padLeft = (bufferSize - windowLength) / 2.  This is
 *            the PyTorch / librosa / torch.stft convention (used by torch.stft
 *            and librosa.stft for win_length < n_fft).
 *          - \c kLeft   : padLeft = 0, zeros on the right.  This is the
 *            scipy.signal / raw-numpy "zero-pad to FFT size" convention and the
 *            convention used by conv-based STFTs (e.g. DeepComplexCRN ConvSTFT).
 *          When \p windowLength == \p bufferSize the placement is irrelevant.
 */
enum class WindowAlign : uint32_t {
   kCenter = 0,
   kLeft   = 1
};

namespace detail {

/** @brief Pi as a double; avoids relying on M_PI (not defined by MSVC by default). */
constexpr double kWindowPi = 3.14159265358979323846;

} /* namespace detail */

/**
 * @brief Generate a window function.
 *
 * @details Fills \p pWindow with \p windowSize samples of the requested window
 *          type. When \p periodic is true a DFT-even denominator (N) is used
 *          (suitable for spectral analysis / STFT); when false the symmetric
 *          denominator (N-1) is used. Bartlett is always symmetric and ignores
 *          \p periodic (matching numpy.bartlett).
 *
 *          The \p power parameter raises every sample to the given exponent
 *          after generation. Use \c 0.5 for a sqrt-window. Default is \c 1.0
 *          (identity).
 *
 *          Coefficients match the Python references used for test-data
 *          generation: torch.{hann,hamming,blackman}_window(periodic=...),
 *          scipy.signal.windows.blackmanharris(sym=...), and numpy.bartlett.
 *
 * @tparam T  Sample type (float or double).
 *
 * @param[out] pWindow     Caller-allocated output buffer of length \p windowSize.
 * @param[in]  windowSize  Number of window points (N).
 * @param[in]  type        Window function to generate.
 * @param[in]  periodic    True for DFT-even (denominator N), false for
 *                         symmetric (denominator N-1).
 * @param[in]  power       Exponent applied to each sample. Default 1.0.
 */
template <typename T>
void windowGen(T *pWindow, uint32_t windowSize, WindowType type, bool periodic, T power = static_cast<T>(1))
{
   if ((pWindow == nullptr) || (windowSize == 0u)) {
      return;
   }

   /* cosine-window denominator: N (periodic) or N-1 (symmetric) */
   const double denom = periodic ? static_cast<double>(windowSize)
                                  : static_cast<double>(windowSize - 1u);
   /* bartlett denominator is always symmetric (N-1) */
   const double bartlettDenom = (windowSize > 1u) ? static_cast<double>(windowSize - 1u) : 1.0;

   for (uint32_t n = 0; n < windowSize; n++) {
      const double x = (denom > 0.0) ? (2.0 * detail::kWindowPi * static_cast<double>(n) / denom) : 0.0;
      double       w;

      switch (type) {
      case WindowType::kRectangular:
         w = 1.0;
         break;
      case WindowType::kHann:
         w = 0.5 - 0.5 * std::cos(x);
         break;
      case WindowType::kHamming:
         w = 0.54 - 0.46 * std::cos(x);
         break;
      case WindowType::kBlackman:
         w = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
         break;
      case WindowType::kBlackmanHarris:
         w = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2.0 * x) - 0.01168 * std::cos(3.0 * x);
         break;
      case WindowType::kBartlett:
         w = 1.0 - std::fabs((2.0 * static_cast<double>(n) / bartlettDenom) - 1.0);
         break;
      case WindowType::kSqrtHann:
         w = std::sqrt(0.5 - 0.5 * std::cos(x));
         break;
      default:
         w = 1.0;
         break;
      }

      if (power != static_cast<T>(1)) {
         w = std::pow(w, static_cast<double>(power));
      }

      pWindow[n] = static_cast<T>(w);
   }
}

/**
 * @brief Generate a window placed inside a larger (zero-padded) buffer.
 *
 * @details Generates \p windowLength coefficients positioned within a
 *          \p bufferSize-long buffer according to \p align, with zeros on the
 *          unused edge(s). When \p windowLength == \p bufferSize this is
 *          identical to the single-size overload.
 *
 * @pre windowLength <= bufferSize.
 *
 * @tparam T  Sample type (float or double).
 *
 * @param[out] pWindow      Caller-allocated output buffer of length \p bufferSize.
 * @param[in]  bufferSize   Total buffer size (e.g. fftSize).
 * @param[in]  windowLength Number of non-zero window coefficients.
 * @param[in]  type         Window function type.
 * @param[in]  periodic     True for DFT-even, false for symmetric.
 * @param[in]  align        Placement of the window within the buffer
 *                          (kCenter = PyTorch/librosa convention,
 *                           kLeft = scipy/numpy/conv-STFT convention).
 *                          Default kCenter.
 * @param[in]  power        Exponent applied to each coefficient. Default 1.0.
 */
template <typename T>
void windowGen(T *pWindow, uint32_t bufferSize, uint32_t windowLength, WindowType type, bool periodic,
               WindowAlign align = WindowAlign::kCenter, T power = static_cast<T>(1))
{
   if ((pWindow == nullptr) || (bufferSize == 0u) || (windowLength > bufferSize)) {
      return;
   }

   /* zero the whole buffer first, then place the window coefficients */
   for (uint32_t i = 0; i < bufferSize; i++) {
      pWindow[i] = static_cast<T>(0);
   }

   const uint32_t padLeft = (align == WindowAlign::kLeft) ? 0u : ((bufferSize - windowLength) / 2u);

   windowGen<T>(&pWindow[padLeft], windowLength, type, periodic, power);
}

} /* namespace TISP */
