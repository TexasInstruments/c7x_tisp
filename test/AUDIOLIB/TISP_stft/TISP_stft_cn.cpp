// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_stft_cn.h"

#include "TISP_common.hpp"

#include <cmath>
#include <cstring>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/*
 * direct complex DFT, valid for any length n (not only powers of two).
 *
 * computes the normalized DFT: X[k] = sum_n x[n] * e^(-j*2*pi*n*k/n).
 * O(n^2), but this is a golden reference so correctness for arbitrary n wins over
 * speed. it replaces a radix-2 decimation-in-time version that only handled
 * power-of-two n and overran its length-n buffers for mixed-radix sizes (e.g. 320).
 *
 * re[n], im[n] : real/imag parts, length n; hold the time-domain signal on entry
 * and the frequency-domain result on exit (real part in re, imag in im).
 */
static void fftDit(float *re, float *im, uint32_t n)
{
   auto outRe = std::unique_ptr<float[]>(new float[n]);
   auto outIm = std::unique_ptr<float[]>(new float[n]);

   for (uint32_t k = 0; k < n; k++) {
      double accR = 0.0;
      double accI = 0.0;

      for (uint32_t n_idx = 0; n_idx < n; n_idx++) {
         double theta = -2.0 * M_PI * (double) n_idx * (double) k / (double) n;
         double wR    = cos(theta);
         double wI    = sin(theta);

         accR += (double)re[n_idx] * wR - (double)im[n_idx] * wI;
         accI += (double)re[n_idx] * wI + (double)im[n_idx] * wR;
      }

      outRe[k] = (float)accR;
      outIm[k] = (float)accI;
   }

   for (uint32_t k = 0; k < n; k++) {
      re[k] = outRe[k];
      im[k] = outIm[k];
   }
}


void AUDIOLIB_stft_cn(float    *pX,
                      float    *pY,
                      float    *pWindow,
                      float    *pState,
                      float    *pScratch,
                      uint32_t  fftSize,
                      uint32_t  hopSize,
                      uint32_t  numChannels,
                      uint32_t  numInputSamples,
                      bool      onesided)
{
   uint32_t outBins = onesided ? (fftSize + 2u) : (2u * fftSize);

   /* derived constants */
   uint32_t overlapBufSize;
   uint32_t numWindows;

   if (numInputSamples < fftSize) {
      overlapBufSize = fftSize - hopSize;
   }
   else {
      uint32_t firstFrameWindows = (numInputSamples - fftSize) / hopSize + 1u;
      overlapBufSize             = numInputSamples - (firstFrameWindows * hopSize);
   }

   numWindows = (overlapBufSize + numInputSamples - fftSize) / hopSize + 1u;

   /* use caller-provided scratch for staging (second half, 64-byte aligned) */
   constexpr uint32_t alignElems  = TISP_SE_SA_ALIGN_BYTES / sizeof(float);
   uint32_t firstHalfSize = ((numWindows * fftSize) + alignElems - 1u) & ~(alignElems - 1u);
   float *staging = &pScratch[firstHalfSize];

   /* allocate fft work buffers */
   auto fftRe = std::unique_ptr<float[]>(new float[fftSize]);
   auto fftIm = std::unique_ptr<float[]>(new float[fftSize]);

   for (uint32_t chIdx = 0; chIdx < numChannels; chIdx++) {

      /* concatenate overlap state and new input into staging buffer */
      memcpy(staging, &pState[chIdx * overlapBufSize], overlapBufSize * sizeof(float));
      memcpy(&staging[overlapBufSize], &pX[chIdx * numInputSamples], numInputSamples * sizeof(float));

      /* process each frame */
      for (uint32_t frameIdx = 0; frameIdx < numWindows; frameIdx++) {

         /* window and prepare fft input */
         for (uint32_t sampleIdx = 0; sampleIdx < fftSize; sampleIdx++) {
            fftRe[sampleIdx] = staging[frameIdx * hopSize + sampleIdx] * pWindow[sampleIdx];
            fftIm[sampleIdx] = 0.0f;
         }

         /* compute fft */
         fftDit(fftRe.get(), fftIm.get(), fftSize);

         /* write output in complex interleaved format */
         uint32_t binsToWrite = onesided ? (fftSize / 2u + 1u) : fftSize;

         for (uint32_t binIdx = 0; binIdx < binsToWrite; binIdx++) {
            pY[chIdx * numWindows * outBins + frameIdx * outBins + 2u * binIdx]      = fftRe[binIdx];
            pY[chIdx * numWindows * outBins + frameIdx * outBins + 2u * binIdx + 1u] = fftIm[binIdx];
         }
      }

      /* save unconsumed tail */
      memcpy(&pState[chIdx * overlapBufSize], &staging[numWindows * hopSize], overlapBufSize * sizeof(float));
   }
}
