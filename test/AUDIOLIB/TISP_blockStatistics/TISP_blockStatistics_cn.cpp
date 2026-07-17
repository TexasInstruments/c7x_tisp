// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_blockStatistics_cn.h"
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

template <typename dataType>
void AUDIOLIB_blockStatistics_max_exec_cn(void  *pIn,
                                          void  *pOut,
                                          size_t widthIn,
                                          size_t heightIn,
                                          size_t strideInElements,
                                          size_t strideOutElements)
{

   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_max_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_min_exec_cn(void  *pIn,
                                          void  *pOut,
                                          size_t widthIn,
                                          size_t heightIn,
                                          size_t strideInElements,
                                          size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_min_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_maxAbs_exec_cn(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t strideInElements,
                                             size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_maxAbs_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_mean_exec_cn(void  *pIn,
                                           void  *pOut,
                                           size_t widthIn,
                                           size_t heightIn,
                                           size_t strideInElements,
                                           size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_mean_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_rms_exec_cn(void  *pIn,
                                          void  *pOut,
                                          size_t widthIn,
                                          size_t heightIn,
                                          size_t strideInElements,
                                          size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_rms_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_stdDev_exec_cn(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t strideInElements,
                                             size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_stdDev_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_variance_exec_cn(void  *pIn,
                                               void  *pOut,
                                               size_t widthIn,
                                               size_t heightIn,
                                               size_t strideInElements,
                                               size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_var_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_avgEnergy_exec_cn(void  *pIn,
                                                void  *pOut,
                                                size_t widthIn,
                                                size_t heightIn,
                                                size_t strideInElements,
                                                size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_avgEnergy_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_add_exec_cn(void  *pIn,
                                          void  *pOut,
                                          size_t widthIn,
                                          size_t heightIn,
                                          size_t strideInElements,
                                          size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_add_exec_cn(channelData, widthIn);
   }

   return;
}

template <typename dataType>
void AUDIOLIB_blockStatistics_sqrAdd_exec_cn(void  *pIn,
                                             void  *pOut,
                                             size_t widthIn,
                                             size_t heightIn,
                                             size_t strideInElements,
                                             size_t strideOutElements)
{
   dataType *pInLocal  = (dataType *) pIn;
   dataType *pOutLocal = (dataType *) pOut;

   for (uint32_t ch = 0; ch < heightIn; ++ch) {
      dataType *channelData             = pInLocal + (ch * strideInElements);
      pOutLocal[ch * strideOutElements] = AUDIOLIB_perChan_sqrAdd_exec_cn(channelData, widthIn);
   }

   return;
}

// explicit instantiation for the different data type versions
template void AUDIOLIB_blockStatistics_max_exec_cn<float>(void  *pIn,
                                                          void  *pOut,
                                                          size_t widthIn,
                                                          size_t heightIn,
                                                          size_t strideInElements,
                                                          size_t strideOutElements);
template void AUDIOLIB_blockStatistics_min_exec_cn<float>(void  *pIn,
                                                          void  *pOut,
                                                          size_t widthIn,
                                                          size_t heightIn,
                                                          size_t strideInElements,
                                                          size_t strideOutElements);
template void AUDIOLIB_blockStatistics_maxAbs_exec_cn<float>(void  *pIn,
                                                             void  *pOut,
                                                             size_t widthIn,
                                                             size_t heightIn,
                                                             size_t strideInElements,
                                                             size_t strideOutElements);
template void AUDIOLIB_blockStatistics_mean_exec_cn<float>(void  *pIn,
                                                           void  *pOut,
                                                           size_t widthIn,
                                                           size_t heightIn,
                                                           size_t strideInElements,
                                                           size_t strideOutElements);
template void AUDIOLIB_blockStatistics_rms_exec_cn<float>(void  *pIn,
                                                          void  *pOut,
                                                          size_t widthIn,
                                                          size_t heightIn,
                                                          size_t strideInElements,
                                                          size_t strideOutElements);
template void AUDIOLIB_blockStatistics_stdDev_exec_cn<float>(void  *pIn,
                                                             void  *pOut,
                                                             size_t widthIn,
                                                             size_t heightIn,
                                                             size_t strideInElements,
                                                             size_t strideOutElements);
template void AUDIOLIB_blockStatistics_variance_exec_cn<float>(void  *pIn,
                                                               void  *pOut,
                                                               size_t widthIn,
                                                               size_t heightIn,
                                                               size_t strideInElements,
                                                               size_t strideOutElements);
template void AUDIOLIB_blockStatistics_avgEnergy_exec_cn<float>(void  *pIn,
                                                                void  *pOut,
                                                                size_t widthIn,
                                                                size_t heightIn,
                                                                size_t strideInElements,
                                                                size_t strideOutElements);
template void AUDIOLIB_blockStatistics_add_exec_cn<float>(void  *pIn,
                                                          void  *pOut,
                                                          size_t widthIn,
                                                          size_t heightIn,
                                                          size_t strideInElements,
                                                          size_t strideOutElements);
template void AUDIOLIB_blockStatistics_sqrAdd_exec_cn<float>(void  *pIn,
                                                             void  *pOut,
                                                             size_t widthIn,
                                                             size_t heightIn,
                                                             size_t strideInElements,
                                                             size_t strideOutElements);
template void AUDIOLIB_blockStatistics_max_exec_cn<double>(void  *pIn,
                                                           void  *pOut,
                                                           size_t widthIn,
                                                           size_t heightIn,
                                                           size_t strideInElements,
                                                           size_t strideOutElements);
template void AUDIOLIB_blockStatistics_min_exec_cn<double>(void  *pIn,
                                                           void  *pOut,
                                                           size_t widthIn,
                                                           size_t heightIn,
                                                           size_t strideInElements,
                                                           size_t strideOutElements);
template void AUDIOLIB_blockStatistics_maxAbs_exec_cn<double>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t strideInElements,
                                                              size_t strideOutElements);
template void AUDIOLIB_blockStatistics_mean_exec_cn<double>(void  *pIn,
                                                            void  *pOut,
                                                            size_t widthIn,
                                                            size_t heightIn,
                                                            size_t strideInElements,
                                                            size_t strideOutElements);
template void AUDIOLIB_blockStatistics_rms_exec_cn<double>(void  *pIn,
                                                           void  *pOut,
                                                           size_t widthIn,
                                                           size_t heightIn,
                                                           size_t strideInElements,
                                                           size_t strideOutElements);
template void AUDIOLIB_blockStatistics_stdDev_exec_cn<double>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t strideInElements,
                                                              size_t strideOutElements);
template void AUDIOLIB_blockStatistics_variance_exec_cn<double>(void  *pIn,
                                                                void  *pOut,
                                                                size_t widthIn,
                                                                size_t heightIn,
                                                                size_t strideInElements,
                                                                size_t strideOutElements);
template void AUDIOLIB_blockStatistics_avgEnergy_exec_cn<double>(void  *pIn,
                                                                 void  *pOut,
                                                                 size_t widthIn,
                                                                 size_t heightIn,
                                                                 size_t strideInElements,
                                                                 size_t strideOutElements);
template void AUDIOLIB_blockStatistics_add_exec_cn<double>(void  *pIn,
                                                           void  *pOut,
                                                           size_t widthIn,
                                                           size_t heightIn,
                                                           size_t strideInElements,
                                                           size_t strideOutElements);
template void AUDIOLIB_blockStatistics_sqrAdd_exec_cn<double>(void  *pIn,
                                                              void  *pOut,
                                                              size_t widthIn,
                                                              size_t heightIn,
                                                              size_t strideInElements,
                                                              size_t strideOutElements);

void AUDIOLIB_blockStatistics_cn(void    *pIn,
                                 void    *pOut,
                                 uint32_t data_type,
                                 size_t   widthIn,
                                 size_t   heightIn,
                                 size_t   strideInElements,
                                 size_t   strideOutElements,
                                 size_t   statisticsType)
{
   if (data_type == AUDIOLIB_FLOAT32) {
      if (statisticsType == 0) {
         AUDIOLIB_blockStatistics_max_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (statisticsType == 1) {
         AUDIOLIB_blockStatistics_min_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (statisticsType == 2) {
         AUDIOLIB_blockStatistics_maxAbs_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                        strideOutElements);
      }
      else if (statisticsType == 3) {
         AUDIOLIB_blockStatistics_mean_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                      strideOutElements);
      }
      else if (statisticsType == 4) {
         AUDIOLIB_blockStatistics_rms_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (statisticsType == 5) {
         AUDIOLIB_blockStatistics_stdDev_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                        strideOutElements);
      }
      else if (statisticsType == 6) {
         AUDIOLIB_blockStatistics_variance_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                          strideOutElements);
      }
      else if (statisticsType == 7) {
         AUDIOLIB_blockStatistics_avgEnergy_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                           strideOutElements);
      }
      else if (statisticsType == 8) {
         AUDIOLIB_blockStatistics_add_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements, strideOutElements);
      }
      else if (statisticsType == 9) {
         AUDIOLIB_blockStatistics_sqrAdd_exec_cn<float>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                        strideOutElements);
      }
   }
   else if (data_type == AUDIOLIB_FLOAT64) {
      if (statisticsType == 0) {
         AUDIOLIB_blockStatistics_max_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                      strideOutElements);
      }
      else if (statisticsType == 1) {
         AUDIOLIB_blockStatistics_min_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                      strideOutElements);
      }
      else if (statisticsType == 2) {
         AUDIOLIB_blockStatistics_maxAbs_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                         strideOutElements);
      }
      else if (statisticsType == 3) {
         AUDIOLIB_blockStatistics_mean_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                       strideOutElements);
      }
      else if (statisticsType == 4) {
         AUDIOLIB_blockStatistics_rms_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                      strideOutElements);
      }
      else if (statisticsType == 5) {
         AUDIOLIB_blockStatistics_stdDev_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                         strideOutElements);
      }
      else if (statisticsType == 6) {
         AUDIOLIB_blockStatistics_variance_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                           strideOutElements);
      }
      else if (statisticsType == 7) {
         AUDIOLIB_blockStatistics_avgEnergy_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                            strideOutElements);
      }
      else if (statisticsType == 8) {
         AUDIOLIB_blockStatistics_add_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                      strideOutElements);
      }
      else if (statisticsType == 9) {
         AUDIOLIB_blockStatistics_sqrAdd_exec_cn<double>(pIn, pOut, widthIn, heightIn, strideInElements,
                                                         strideOutElements);
      }
   }
}
