#pragma once

#include <cmath>
#include <cstdint>

namespace TISP {

/**
 * @brief Mel-frequency warping convention.
 *
 * @details Selects the Hz<->mel mapping used to place the triangular mel
 *          filters:
 *          - \c kHtk    : the HTK formula mel = 2595 * log10(1 + hz/700).
 *          - \c kSlaney : the Slaney/auditory-toolbox piecewise formula
 *            (linear below 1 kHz, logarithmic above).
 *          Both match the corresponding \c mel_scale option of
 *          torchaudio.functional.melscale_fbanks.
 */
enum class MelScale : uint32_t {
   kHtk    = 0,
   kSlaney = 1
};

/**
 * @brief Mel filterbank area normalisation.
 *
 * @details
 *          - \c kNone   : raw triangular filters (torchaudio norm=None).
 *          - \c kSlaney : each filter scaled by 2/(f_pts[m+2]-f_pts[m]) so the
 *            filters have unit area (torchaudio norm="slaney").
 */
enum class MelNorm : uint32_t {
   kNone   = 0,
   kSlaney = 1
};

namespace detail {

/** @brief Slaney warping constants (linear-region slope and linear/log breakpoint). */
constexpr double kMelSlaneyFSp      = 200.0 / 3.0; /*!< linear-region Hz-per-mel */
constexpr double kMelSlaneyMinLogHz = 1000.0;      /*!< linear/log breakpoint (Hz) */

/** @brief Convert Hz to mel using the selected warping (double precision). */
inline double hzToMel(double hz, MelScale scale)
{
   if (scale == MelScale::kHtk) {
      return 2595.0 * std::log10(1.0 + (hz / 700.0));
   }

   /* Slaney: linear below 1 kHz, logarithmic above */
   const double minLogMel = kMelSlaneyMinLogHz / kMelSlaneyFSp;
   const double logStep   = std::log(6.4) / 27.0;
   double       mel       = hz / kMelSlaneyFSp;

   if (hz >= kMelSlaneyMinLogHz) {
      mel = minLogMel + (std::log(hz / kMelSlaneyMinLogHz) / logStep);
   }

   return mel;
}

/** @brief Convert mel to Hz using the selected warping (double precision). */
inline double melToHz(double mel, MelScale scale)
{
   if (scale == MelScale::kHtk) {
      return 700.0 * (std::pow(10.0, mel / 2595.0) - 1.0);
   }

   /* Slaney inverse */
   const double minLogMel = kMelSlaneyMinLogHz / kMelSlaneyFSp;
   const double logStep   = std::log(6.4) / 27.0;
   double       hz        = kMelSlaneyFSp * mel;

   if (mel >= minLogMel) {
      hz = kMelSlaneyMinLogHz * std::exp(logStep * (mel - minLogMel));
   }

   return hz;
}

} /* namespace detail */

/**
 * @brief Generate the mel filterbank weight matrix.
 *
 * @details Fills \p pMel with an [\p numSpectrogramBins x \p numMelBins] matrix
 *          in **bin-major (row-major) order**: element (bin, mel) lives at
 *          `pMel[bin * numMelBins + mel]`. Post-multiplying an
 *          [frames x numSpectrogramBins] magnitude/power spectrogram by this
 *          matrix yields the [frames x numMelBins] mel spectrogram.
 *
 *          The construction reproduces torchaudio.functional.melscale_fbanks
 *          (the matrix torchaudio.transforms.MelScale applies internally, used by
 *          the audioai-vggish and yamnet-audioset front-ends): frequency bins are
 *          linearly spaced on [0, sampleRate/2]; \p numMelBins+2 edge points are
 *          evenly spaced in mel and mapped back to Hz; each filter is the clamped
 *          minimum of its rising and falling slopes. Internal math is double
 *          precision, matching \c windowGen; the result is cast to \p T.
 *
 * @tparam T  Coefficient type (float or double).
 *
 * @param[out] pMel               Caller-allocated buffer of
 *                                \p numSpectrogramBins * \p numMelBins elements.
 * @param[in]  numSpectrogramBins Number of one-sided FFT bins (fftSize/2 + 1).
 * @param[in]  numMelBins         Number of mel bands.
 * @param[in]  sampleRate         Audio sample rate in Hz (Nyquist uses sampleRate/2,
 *                                integer division, matching torchaudio).
 * @param[in]  fMin               Lowest mel-band edge in Hz.
 * @param[in]  fMax               Highest mel-band edge in Hz (typically <= sampleRate/2).
 * @param[in]  scale              Hz<->mel warping (kHtk or kSlaney). Default kHtk.
 * @param[in]  norm               Area normalisation (kNone or kSlaney). Default kNone.
 *
 * @pre  fMax > fMin and the resulting mel edge points are strictly increasing
 *       (true for all standard configurations); degenerate parameters that make
 *       adjacent edges coincide would divide by zero, matching torchaudio's own
 *       unguarded behaviour.
 */
template <typename T>
void melFbankGen(T       *pMel,
                 uint32_t numSpectrogramBins,
                 uint32_t numMelBins,
                 uint32_t sampleRate,
                 T        fMin,
                 T        fMax,
                 MelScale scale = MelScale::kHtk,
                 MelNorm  norm  = MelNorm::kNone)
{
   if ((pMel == nullptr) || (numSpectrogramBins == 0u) || (numMelBins == 0u)) {
      return;
   }

   const double nyquist = static_cast<double>(sampleRate / 2u);
   const double mMin    = detail::hzToMel(static_cast<double>(fMin), scale);
   const double mMax    = detail::hzToMel(static_cast<double>(fMax), scale);
   const double melStep = (mMax - mMin) / static_cast<double>(numMelBins + 1u);

   for (uint32_t m = 0; m < numMelBins; m++) {
      /* three consecutive mel edge points -> Hz, computed once per filter */
      const double fp0 = detail::melToHz(mMin + (melStep * static_cast<double>(m)), scale);
      const double fp1 = detail::melToHz(mMin + (melStep * static_cast<double>(m + 1u)), scale);
      const double fp2 = detail::melToHz(mMin + (melStep * static_cast<double>(m + 2u)), scale);

      const double downDen    = fp1 - fp0;
      const double upDen      = fp2 - fp1;
      const double normFactor = (norm == MelNorm::kSlaney) ? (2.0 / (fp2 - fp0)) : 1.0;

      for (uint32_t bin = 0; bin < numSpectrogramBins; bin++) {
         const double f    = (numSpectrogramBins > 1u)
                                 ? (nyquist * static_cast<double>(bin) / static_cast<double>(numSpectrogramBins - 1u))
                                 : 0.0;
         const double down = -(fp0 - f) / downDen;
         const double up   = (fp2 - f) / upDen;
         double       w    = (down < up) ? down : up;

         w = (w > 0.0) ? w : 0.0;
         w *= normFactor;

         pMel[(bin * numMelBins) + m] = static_cast<T>(w);
      }
   }
}

} /* namespace TISP */
