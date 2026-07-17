// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_subBlockStatistics_cn.h"
#include <audiolib.h>
#include <cmath>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

// Templatized inline functions for per-channel statistics
template <typename dataType> inline dataType AUDIOLIB_perChan_max_exec_cn(const dataType *data, uint32_t size)
{
   if (size == 0)
      return dataType(0);
   dataType max_val = data[0];
   for (uint32_t i = 1; i < size; ++i) {
      if (data[i] > max_val)
         max_val = data[i];
   }
   return max_val;
}

template <typename dataType> inline dataType AUDIOLIB_perChan_min_exec_cn(const dataType *data, uint32_t size)
{
   if (size == 0)
      return dataType(0);
   dataType min_val = data[0];
   for (uint32_t i = 1; i < size; ++i) {
      if (data[i] < min_val)
         min_val = data[i];
   }
   return min_val;
}

template <typename dataType> inline dataType AUDIOLIB_perChan_maxAbs_exec_cn(const dataType *data, uint32_t size)
{
   if (size == 0)
      return dataType(0);
   dataType max_abs = std::abs(data[0]);
   for (uint32_t i = 1; i < size; ++i) {
      dataType abs_val = std::abs(data[i]);
      if (abs_val > max_abs)
         max_abs = abs_val;
   }
   return max_abs;
}

template <typename dataType> inline dataType AUDIOLIB_perChan_mean_exec_cn(const dataType *data, uint32_t size)
{
   if (size == 0)
      return dataType(0);
   dataType sum = dataType(0);
   for (uint32_t i = 0; i < size; ++i) {
      sum += data[i];
   }
   return sum / static_cast<dataType>(size);
}

template <typename dataType> inline dataType AUDIOLIB_perChan_rms_exec_cn(const dataType *data, uint32_t size)
{
   if (size == 0)
      return dataType(0);
   dataType sum_squares = dataType(0);
   for (uint32_t i = 0; i < size; ++i) {
      sum_squares += data[i] * data[i];
   }
   return std::sqrt(sum_squares / static_cast<dataType>(size));
}

template <typename dataType> inline dataType AUDIOLIB_perChan_stdDev_exec_cn(const dataType *data, uint32_t size)
{
   if (size <= 1)
      return dataType(0);
   dataType mean          = AUDIOLIB_perChan_mean_exec_cn(data, size);
   dataType sumSquaredDev = dataType(0);
   for (uint32_t i = 0; i < size; ++i) {
      dataType dev = data[i] - mean;
      sumSquaredDev += dev * dev;
   }
   return std::sqrt(sumSquaredDev / static_cast<dataType>(size));
}

template <typename dataType> inline dataType AUDIOLIB_perChan_var_exec_cn(const dataType *data, uint32_t size)
{
   if (size <= 1)
      return dataType(0);
   dataType mean          = AUDIOLIB_perChan_mean_exec_cn(data, size);
   dataType sumSquaredDev = dataType(0);
   for (uint32_t i = 0; i < size; ++i) {
      dataType dev = data[i] - mean;
      sumSquaredDev += dev * dev;
   }
   return sumSquaredDev / static_cast<dataType>(size);
}

template <typename dataType> inline dataType AUDIOLIB_perChan_avgEnergy_exec_cn(const dataType *data, uint32_t size)
{
   if (size == 0)
      return dataType(0);
   dataType sum_squares = dataType(0);
   for (uint32_t i = 0; i < size; ++i) {
      sum_squares += data[i] * data[i];
   }
   return sum_squares / static_cast<dataType>(size);
}

template <typename dataType> inline dataType AUDIOLIB_perChan_add_exec_cn(const dataType *data, uint32_t size)
{
   if (size == 0)
      return dataType(0);
   dataType sum = dataType(0);
   for (uint32_t i = 0; i < size; ++i) {
      sum += data[i];
   }
   return sum;
}

template <typename dataType> inline dataType AUDIOLIB_perChan_sqrAdd_exec_cn(const dataType *data, uint32_t size)
{
   if (size == 0)
      return dataType(0);
   dataType sum_squares = dataType(0);
   for (uint32_t i = 0; i < size; ++i) {
      sum_squares += data[i] * data[i];
   }
   return sum_squares;
}

// Templatized inline functions for all-channel statistics
template <typename dataType>
inline dataType AUDIOLIB_allChan_max_exec_cn(const dataType *pInLocal,
                                             uint32_t        subBlockSize,
                                             uint32_t        channels,
                                             uint32_t        strideIn,
                                             uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0)
      return dataType(0);
   dataType max_val = pInLocal[0 * strideIn + subBlockIdx * subBlockSize];
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         dataType val = pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j];
         if (val > max_val)
            max_val = val;
      }
   }
   return max_val;
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_min_exec_cn(const dataType *pInLocal,
                                             uint32_t        subBlockSize,
                                             uint32_t        channels,
                                             uint32_t        strideIn,
                                             uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0)
      return dataType(0);
   dataType min_val = pInLocal[0 * strideIn + subBlockIdx * subBlockSize];
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         dataType val = pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j];
         if (val < min_val)
            min_val = val;
      }
   }
   return min_val;
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_maxAbs_exec_cn(const dataType *pInLocal,
                                                uint32_t        subBlockSize,
                                                uint32_t        channels,
                                                uint32_t        strideIn,
                                                uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0)
      return dataType(0);
   dataType max_abs = std::abs(pInLocal[0 * strideIn + subBlockIdx * subBlockSize]);
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         dataType abs_val = std::abs(pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j]);
         if (abs_val > max_abs)
            max_abs = abs_val;
      }
   }
   return max_abs;
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_mean_exec_cn(const dataType *pInLocal,
                                              uint32_t        subBlockSize,
                                              uint32_t        channels,
                                              uint32_t        strideIn,
                                              uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0)
      return dataType(0);
   dataType sum          = dataType(0);
   uint32_t totalSamples = subBlockSize * channels;
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         sum += pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j];
      }
   }
   return sum / static_cast<dataType>(totalSamples);
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_rms_exec_cn(const dataType *pInLocal,
                                             uint32_t        subBlockSize,
                                             uint32_t        channels,
                                             uint32_t        strideIn,
                                             uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0)
      return dataType(0);
   dataType sum_squares  = dataType(0);
   uint32_t totalSamples = subBlockSize * channels;
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         dataType val = pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j];
         sum_squares += val * val;
      }
   }
   return std::sqrt(sum_squares / static_cast<dataType>(totalSamples));
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_stdDev_exec_cn(const dataType *pInLocal,
                                                uint32_t        subBlockSize,
                                                uint32_t        channels,
                                                uint32_t        strideIn,
                                                uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0 || subBlockSize * channels <= 1)
      return dataType(0);
   dataType mean          = AUDIOLIB_allChan_mean_exec_cn(pInLocal, subBlockSize, channels, strideIn, subBlockIdx);
   dataType sumSquaredDev = dataType(0);
   uint32_t totalSamples  = subBlockSize * channels;
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         dataType dev = pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j] - mean;
         sumSquaredDev += dev * dev;
      }
   }
   return std::sqrt(sumSquaredDev / static_cast<dataType>(totalSamples));
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_var_exec_cn(const dataType *pInLocal,
                                             uint32_t        subBlockSize,
                                             uint32_t        channels,
                                             uint32_t        strideIn,
                                             uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0 || subBlockSize * channels <= 1)
      return dataType(0);
   dataType mean          = AUDIOLIB_allChan_mean_exec_cn(pInLocal, subBlockSize, channels, strideIn, subBlockIdx);
   dataType sumSquaredDev = dataType(0);
   uint32_t totalSamples  = subBlockSize * channels;
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         dataType dev = pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j] - mean;
         sumSquaredDev += dev * dev;
      }
   }
   return sumSquaredDev / static_cast<dataType>(totalSamples);
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_avgEnergy_exec_cn(const dataType *pInLocal,
                                                   uint32_t        subBlockSize,
                                                   uint32_t        channels,
                                                   uint32_t        strideIn,
                                                   uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0)
      return dataType(0);
   dataType sum_squares  = dataType(0);
   uint32_t totalSamples = subBlockSize * channels;
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         dataType val = pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j];
         sum_squares += val * val;
      }
   }
   return sum_squares / static_cast<dataType>(totalSamples);
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_add_exec_cn(const dataType *pInLocal,
                                             uint32_t        subBlockSize,
                                             uint32_t        channels,
                                             uint32_t        strideIn,
                                             uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0)
      return dataType(0);
   dataType sum = dataType(0);
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         sum += pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j];
      }
   }
   return sum;
}

template <typename dataType>
inline dataType AUDIOLIB_allChan_sqrAdd_exec_cn(const dataType *pInLocal,
                                                uint32_t        subBlockSize,
                                                uint32_t        channels,
                                                uint32_t        strideIn,
                                                uint32_t        subBlockIdx)
{
   if (subBlockSize == 0 || channels == 0)
      return dataType(0);
   dataType sum_squares = dataType(0);
   for (uint32_t ch = 0; ch < channels; ++ch) {
      for (uint32_t j = 0; j < subBlockSize; ++j) {
         dataType val = pInLocal[ch * strideIn + subBlockIdx * subBlockSize + j];
         sum_squares += val * val;
      }
   }
   return sum_squares;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_max_exec_cn(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t subBlockSize,
                                             size_t strideInElements,
                                             size_t strideOutElements,
                                             size_t flagPerChanStats)
{

   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_max_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_max_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_min_exec_cn(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t subBlockSize,
                                             size_t strideInElements,
                                             size_t strideOutElements,
                                             size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_min_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_min_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_maxAbs_exec_cn(void  *pIn,
                                                void  *pOut,
                                                size_t widthIn,
                                                size_t heightIn,
                                                size_t subBlockSize,
                                                size_t strideInElements,
                                                size_t strideOutElements,
                                                size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_maxAbs_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_maxAbs_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_mean_exec_cn(void  *pIn,
                                              void  *pOut,
                                              size_t widthIn,
                                              size_t heightIn,
                                              size_t subBlockSize,
                                              size_t strideInElements,
                                              size_t strideOutElements,
                                              size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_mean_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_mean_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_rms_exec_cn(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t subBlockSize,
                                             size_t strideInElements,
                                             size_t strideOutElements,
                                             size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_rms_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_rms_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_stdDev_exec_cn(void  *pIn,
                                                void  *pOut,
                                                size_t widthIn,
                                                size_t heightIn,
                                                size_t subBlockSize,
                                                size_t strideInElements,
                                                size_t strideOutElements,
                                                size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_stdDev_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_stdDev_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_variance_exec_cn(void  *pIn,
                                                  void  *pOut,
                                                  size_t widthIn,
                                                  size_t heightIn,
                                                  size_t subBlockSize,
                                                  size_t strideInElements,
                                                  size_t strideOutElements,
                                                  size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_var_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_var_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_avgEnergy_exec_cn(void  *pIn,
                                                   void  *pOut,
                                                   size_t widthIn,
                                                   size_t heightIn,
                                                   size_t subBlockSize,
                                                   size_t strideInElements,
                                                   size_t strideOutElements,
                                                   size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_avgEnergy_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_avgEnergy_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_add_exec_cn(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t subBlockSize,
                                             size_t strideInElements,
                                             size_t strideOutElements,
                                             size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_add_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_add_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

template <typename dataType>
void AUDIOLIB_subBlockStatistics_sqrAdd_exec_cn(void  *pIn,
                                                void  *pOut,
                                                size_t widthIn,
                                                size_t heightIn,
                                                size_t subBlockSize,
                                                size_t strideInElements,
                                                size_t strideOutElements,
                                                size_t flagPerChanStats)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   if (flagPerChanStats) {
      // Per-channel statistics
      for (uint32_t ch = 0; ch < heightIn; ++ch) {
         for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
            dataType *subblock                    = pInLocal + (ch * strideInElements + i * subBlockSize);
            pOutLocal[ch * strideOutElements + i] = AUDIOLIB_perChan_sqrAdd_exec_cn(subblock, subBlockSize);
         }
      }
   }
   else {
      // All-channel statistics
      for (uint32_t i = 0; i < (widthIn / subBlockSize); ++i) {
         pOutLocal[i] = AUDIOLIB_allChan_sqrAdd_exec_cn(pInLocal, subBlockSize, heightIn, strideInElements, i);
      }
   }

   return;
}

// explicit instantiation for the different data type versions
template void AUDIOLIB_subBlockStatistics_max_exec_cn<float>(void  *pIn,
                                                             void  *pOut,
                                                             size_t widthIn,
                                                             size_t heightIn,
                                                             size_t subBlockSize,
                                                             size_t strideInElements,
                                                             size_t strideOutElements,
                                                             size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_min_exec_cn<float>(void  *pIn,
                                                             void  *pOut,
                                                             size_t widthIn,
                                                             size_t heightIn,
                                                             size_t subBlockSize,
                                                             size_t strideInElements,
                                                             size_t strideOutElements,
                                                             size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_maxAbs_exec_cn<float>(void  *pIn,
                                                                void  *pOut,
                                                                size_t widthIn,
                                                                size_t heightIn,
                                                                size_t subBlockSize,
                                                                size_t strideInElements,
                                                                size_t strideOutElements,
                                                                size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_mean_exec_cn<float>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t subBlockSize,
                                                              size_t strideInElements,
                                                              size_t strideOutElements,
                                                              size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_rms_exec_cn<float>(void  *pIn,
                                                             void  *pOut,
                                                             size_t widthIn,
                                                             size_t heightIn,
                                                             size_t subBlockSize,
                                                             size_t strideInElements,
                                                             size_t strideOutElements,
                                                             size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_stdDev_exec_cn<float>(void  *pIn,
                                                                void  *pOut,
                                                                size_t widthIn,
                                                                size_t heightIn,
                                                                size_t subBlockSize,
                                                                size_t strideInElements,
                                                                size_t strideOutElements,
                                                                size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_variance_exec_cn<float>(void  *pIn,
                                                                  void  *pOut,
                                                                  size_t widthIn,
                                                                  size_t heightIn,
                                                                  size_t subBlockSize,
                                                                  size_t strideInElements,
                                                                  size_t strideOutElements,
                                                                  size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_avgEnergy_exec_cn<float>(void  *pIn,
                                                                   void  *pOut,
                                                                   size_t widthIn,
                                                                   size_t heightIn,
                                                                   size_t subBlockSize,
                                                                   size_t strideInElements,
                                                                   size_t strideOutElements,
                                                                   size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_add_exec_cn<float>(void  *pIn,
                                                             void  *pOut,
                                                             size_t widthIn,
                                                             size_t heightIn,
                                                             size_t subBlockSize,
                                                             size_t strideInElements,
                                                             size_t strideOutElements,
                                                             size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_sqrAdd_exec_cn<float>(void  *pIn,
                                                                void  *pOut,
                                                                size_t widthIn,
                                                                size_t heightIn,
                                                                size_t subBlockSize,
                                                                size_t strideInElements,
                                                                size_t strideOutElements,
                                                                size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_max_exec_cn<double>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t subBlockSize,
                                                              size_t strideInElements,
                                                              size_t strideOutElements,
                                                              size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_min_exec_cn<double>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t subBlockSize,
                                                              size_t strideInElements,
                                                              size_t strideOutElements,
                                                              size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_maxAbs_exec_cn<double>(void  *pIn,
                                                                 void  *pOut,
                                                                 size_t widthIn,
                                                                 size_t heightIn,
                                                                 size_t subBlockSize,
                                                                 size_t strideInElements,
                                                                 size_t strideOutElements,
                                                                 size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_mean_exec_cn<double>(void  *pIn,
                                                               void  *pOut,
                                                               size_t widthIn,
                                                               size_t heightIn,
                                                               size_t subBlockSize,
                                                               size_t strideInElements,
                                                               size_t strideOutElements,
                                                               size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_rms_exec_cn<double>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t subBlockSize,
                                                              size_t strideInElements,
                                                              size_t strideOutElements,
                                                              size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_stdDev_exec_cn<double>(void  *pIn,
                                                                 void  *pOut,
                                                                 size_t widthIn,
                                                                 size_t heightIn,
                                                                 size_t subBlockSize,
                                                                 size_t strideInElements,
                                                                 size_t strideOutElements,
                                                                 size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_variance_exec_cn<double>(void  *pIn,
                                                                   void  *pOut,
                                                                   size_t widthIn,
                                                                   size_t heightIn,
                                                                   size_t subBlockSize,
                                                                   size_t strideInElements,
                                                                   size_t strideOutElements,
                                                                   size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_avgEnergy_exec_cn<double>(void  *pIn,
                                                                    void  *pOut,
                                                                    size_t widthIn,
                                                                    size_t heightIn,
                                                                    size_t subBlockSize,
                                                                    size_t strideInElements,
                                                                    size_t strideOutElements,
                                                                    size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_add_exec_cn<double>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t subBlockSize,
                                                              size_t strideInElements,
                                                              size_t strideOutElements,
                                                              size_t flagPerChanStats);
template void AUDIOLIB_subBlockStatistics_sqrAdd_exec_cn<double>(void  *pIn,
                                                                 void  *pOut,
                                                                 size_t widthIn,
                                                                 size_t heightIn,
                                                                 size_t subBlockSize,
                                                                 size_t strideInElements,
                                                                 size_t strideOutElements,
                                                                 size_t flagPerChanStats);

void AUDIOLIB_subBlockStatistics_cn(void    *pIn,
                                    void    *pOut,
                                    uint32_t data_type,
                                    size_t   widthIn,
                                    size_t   heightIn,
                                    size_t   subBlockSize,
                                    size_t   strideInElements,
                                    size_t   strideOutElements,
                                    size_t   flagPerChanStats,
                                    size_t   statisticsType)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      if (statisticsType == 0) {
         AUDIOLIB_subBlockStatistics_max_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                        strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 1) {
         AUDIOLIB_subBlockStatistics_min_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                        strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 2) {
         AUDIOLIB_subBlockStatistics_maxAbs_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                           strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 3) {
         AUDIOLIB_subBlockStatistics_mean_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                         strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 4) {
         AUDIOLIB_subBlockStatistics_rms_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                        strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 5) {
         AUDIOLIB_subBlockStatistics_stdDev_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                           strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 6) {
         AUDIOLIB_subBlockStatistics_variance_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize,
                                                             strideInElements, strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 7) {
         AUDIOLIB_subBlockStatistics_avgEnergy_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize,
                                                              strideInElements, strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 8) {
         AUDIOLIB_subBlockStatistics_add_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                        strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 9) {
         AUDIOLIB_subBlockStatistics_sqrAdd_exec_cn<float>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                           strideOutElements, flagPerChanStats);
      }
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      if (statisticsType == 0) {
         AUDIOLIB_subBlockStatistics_max_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                         strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 1) {
         AUDIOLIB_subBlockStatistics_min_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                         strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 2) {
         AUDIOLIB_subBlockStatistics_maxAbs_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize,
                                                            strideInElements, strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 3) {
         AUDIOLIB_subBlockStatistics_mean_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                          strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 4) {
         AUDIOLIB_subBlockStatistics_rms_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                         strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 5) {
         AUDIOLIB_subBlockStatistics_stdDev_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize,
                                                            strideInElements, strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 6) {
         AUDIOLIB_subBlockStatistics_variance_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize,
                                                              strideInElements, strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 7) {
         AUDIOLIB_subBlockStatistics_avgEnergy_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize,
                                                               strideInElements, strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 8) {
         AUDIOLIB_subBlockStatistics_add_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize, strideInElements,
                                                         strideOutElements, flagPerChanStats);
      }
      else if (statisticsType == 9) {
         AUDIOLIB_subBlockStatistics_sqrAdd_exec_cn<double>(pIn, pOut, widthIn, heightIn, subBlockSize,
                                                            strideInElements, strideOutElements, flagPerChanStats);
      }
   }
}
