// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef TISP_PROFILE_HPP_
#define TISP_PROFILE_HPP_

#include <cstdint>

namespace TISP {

/**
 * @brief Enumeration for data organization mode
 */
enum class DataMode {
   kInterleave,  ///< Data is organized as [ch0_s0, ch1_s0, ..., chN_s0, ch0_s1, ...]
   kDeinterleave ///< Data is organized as [ch0_s0, ch0_s1, ..., ch0_sM, ch1_s0, ...]
};

/**
 * @brief Calculate and print SNR values on a per-channel basis
 *
 * This function calculates the Signal-to-Noise Ratio (SNR) between a reference signal
 * and a test signal, and prints the results in a formatted table. It supports both
 * interleaved and deinterleaved data formats, and works with various numeric data types.
 *
 * The function prints a table showing:
 * - SNR values for each channel in dB
 * - Signal power for each channel
 * - Noise power for each channel
 * - Overall SNR across all channels
 *
 * @tparam T Data type of the input signals (must be arithmetic)
 * @param pRef Pointer to reference signal data
 * @param pTest Pointer to test signal data
 * @param numsamples Number of samples per channel
 * @param numchannels Number of channels
 * @param mode Data organization mode (interleave or deinterleave)
 * @return double Overall SNR value across all channels in dB
 *
 * @note The function supports both interleaved and deinterleaved data formats:
 *   - kInterleave: Data is organized as [ch0_s0, ch1_s0, ..., chN_s0, ch0_s1, ...]
 *   - kDeinterleave: Data is organized as [ch0_s0, ch0_s1, ..., ch0_sM, ch1_s0, ...]
 *
 * @example
 * // Example with int32_t data type and interleaved format
 * int32_t reference_signal[1024*4]; // 4 channels, 1024 samples each
 * int32_t test_signal[1024*4];
 * // ... fill the arrays with data ...
 * double overall_snr = TISP::print_snr(
 *     reference_signal,
 *     test_signal,
 *     1024,           // samples per channel
 *     4,              // number of channels
 *     TISP::DataMode::kInterleave
 * );
 *
 * @example
 * // Example with float data type and deinterleaved format
 * float reference_signal[1024*8]; // 8 channels, 1024 samples each
 * float test_signal[1024*8];
 * // ... fill the arrays with data ...
 * double overall_snr = TISP::print_snr(
 *     reference_signal,
 *     test_signal,
 *     1024,           // samples per channel
 *     8,              // number of channels
 *     TISP::DataMode::kDeinterleave
 * );
 */
template <typename T>
double print_snr(const T* pRef, const T* pTest, uint32_t numsamples, uint32_t numchannels, DataMode mode);

} // namespace TISP

#endif // TISP_PROFILE_HPP_
