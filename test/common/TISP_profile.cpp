// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/**
 * @file TISP_profile.cpp
 * @brief Implementation of the TISP profiling functions
 */

#include "TISP_profile.hpp"
#include <cmath>
#include <cstdio>
#include <type_traits>
#include <vector>

namespace TISP {

/**
 * @brief Structure to hold SNR analysis results (internal use only)
 */
struct SNRResults {
   std::vector<double> snr_values;           ///< SNR values for each channel in dB
   std::vector<double> signal_power;         ///< Signal power for each channel
   std::vector<double> noise_power;          ///< Noise power for each channel
   double              overall_snr;          ///< Overall SNR across all channels in dB
   double              overall_signal_power; ///< Overall signal power
   double              overall_noise_power;  ///< Overall noise power
};

/**
 * @brief Print SNR values in a fancy formatted table (internal use only)
 *
 * @param results SNR analysis results to print
 */
/**
 * @brief Calculate SNR values on a per-channel basis (internal use only)
 *
 * @tparam T Data type of the input signals (must be arithmetic)
 * @param pRef Pointer to reference signal data
 * @param pTest Pointer to test signal data
 * @param numsamples Number of samples per channel
 * @param numchannels Number of channels
 * @param mode Data organization mode (interleave or deinterleave)
 * @return SNRResults Structure containing SNR values and related metrics
 */
template <typename T>
SNRResults calculate_snr(const T* pRef, const T* pTest, uint32_t numsamples, uint32_t numchannels, DataMode mode)
{
   static_assert(std::is_arithmetic<T>::value, "Type T must be arithmetic");

   SNRResults results;
   results.snr_values.resize(numchannels);
   results.signal_power.resize(numchannels);
   results.noise_power.resize(numchannels);

   // Temporary storage for deinterleaved data
   std::vector<double> ref_data(numsamples * numchannels);
   std::vector<double> test_data(numsamples * numchannels);
   std::vector<double> noise_data(numsamples * numchannels);

   // Initialize arrays
   for (uint32_t ch = 0; ch < numchannels; ch++) {
      results.signal_power[ch] = 0.0;
      results.noise_power[ch]  = 0.0;
   }

   // Reshape data based on mode
   if (mode == DataMode::kInterleave) {
      // Reshape interleaved data to deinterleaved format for easier processing
      for (uint32_t i = 0; i < numsamples * numchannels; i++) {
         uint32_t sample_idx  = i / numchannels;
         uint32_t channel_idx = i % numchannels;

         if (sample_idx < numsamples) {
            ref_data[channel_idx * numsamples + sample_idx]  = static_cast<double>(pRef[i]);
            test_data[channel_idx * numsamples + sample_idx] = static_cast<double>(pTest[i]);
         }
      }
   }
   else {
      // Data is already deinterleaved, just copy and convert to double
      for (uint32_t ch = 0; ch < numchannels; ch++) {
         uint32_t start_idx = ch * numsamples;

         for (uint32_t s = 0; s < numsamples; s++) {
            ref_data[ch * numsamples + s]  = static_cast<double>(pRef[start_idx + s]);
            test_data[ch * numsamples + s] = static_cast<double>(pTest[start_idx + s]);
         }
      }
   }

   // Calculate noise as the difference between test and reference
   for (uint32_t i = 0; i < numsamples * numchannels; i++) {
      noise_data[i] = test_data[i] - ref_data[i];
   }

   // Calculate signal power and noise power for each channel
   results.overall_signal_power = 0.0;
   results.overall_noise_power  = 0.0;

   for (uint32_t ch = 0; ch < numchannels; ch++) {
      uint32_t start_idx = ch * numsamples;
      uint32_t end_idx   = start_idx + numsamples;

      for (uint32_t i = start_idx; i < end_idx; i++) {
         // Calculate signal power
         results.signal_power[ch] += (ref_data[i] * ref_data[i]) / numsamples;

         // Calculate noise power
         results.noise_power[ch] += (noise_data[i] * noise_data[i]) / numsamples;

         // Accumulate for overall calculations
         results.overall_signal_power += (ref_data[i] * ref_data[i]);
         results.overall_noise_power += (noise_data[i] * noise_data[i]);
      }
   }

   // Calculate overall power values
   results.overall_signal_power /= (numsamples * numchannels);
   results.overall_noise_power /= (numsamples * numchannels);

   // Calculate SNR in dB for each channel
   for (uint32_t ch = 0; ch < numchannels; ch++) {
      // Add small value to avoid division by zero
      results.snr_values[ch] = 10.0 * std::log10(results.signal_power[ch] / (results.noise_power[ch]));
   }

   // Calculate overall SNR
   results.overall_snr = 10.0 * std::log10(results.overall_signal_power / (results.overall_noise_power));

   return results;
}

void print_snr_results(const SNRResults& results)
{
   const int numchannels = static_cast<int>(results.snr_values.size());

   // Print header
   printf("\n============================================================\n");
   printf("                   SNR ANALYSIS RESULTS                    \n");
   printf("============================================================\n");

   // Print column headers
   printf("%-10s%-15s%-20s%-15s\n", "Channel", "SNR (dB)", "Signal Power", "Noise Power");
   printf("------------------------------------------------------------\n");

   // Print per-channel results
   for (int ch = 0; ch < numchannels; ch++) {
      printf("%-10d%4.2f dB%16.2e%18.2e\n", ch, results.snr_values[ch], results.signal_power[ch],
             results.noise_power[ch]);
   }

   // Print separator
   printf("------------------------------------------------------------\n");

   // Print overall results
   printf("%-10s%4.2f dB%16.2e%18.2e\n", "Overall", results.overall_snr, results.overall_signal_power,
          results.overall_noise_power);

   // Print footer
   printf("============================================================\n\n");
}

/**
 * @brief Calculate and print SNR values on a per-channel basis
 *
 * @tparam T Data type of the input signals (must be arithmetic)
 * @param pRef Pointer to reference signal data
 * @param pTest Pointer to test signal data
 * @param numsamples Number of samples per channel
 * @param numchannels Number of channels
 * @param mode Data organization mode (interleave or deinterleave)
 * @return double Overall SNR value across all channels in dB
 */
template <typename T>
double print_snr(const T* pRef, const T* pTest, uint32_t numsamples, uint32_t numchannels, DataMode mode)
{
   SNRResults results = calculate_snr(pRef, pTest, numsamples, numchannels, mode);
   print_snr_results(results);
   return results.overall_snr;
}

// Explicit template instantiations for common types
template double print_snr<int32_t>(const int32_t*, const int32_t*, uint32_t, uint32_t, DataMode);
template double print_snr<float>(const float*, const float*, uint32_t, uint32_t, DataMode);
template double print_snr<double>(const double*, const double*, uint32_t, uint32_t, DataMode);

} // namespace TISP
