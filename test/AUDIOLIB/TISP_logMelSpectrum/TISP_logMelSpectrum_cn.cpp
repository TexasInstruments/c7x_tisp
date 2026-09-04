// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: Apache-2.0

#include "TISP_logMelSpectrum_cn.h"

#include <cmath>
#include <cstring>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/*
 * radix-2 decimation-in-time complex fft, in-place
 *
 * re[n], im[n] : real/imag parts of input/output, length n (must be power of 2)
 */
static void fftDit(float *re, float *im, uint32_t n)
{
   /* bit reversal permutation */
   uint32_t j = 0;

   for (uint32_t i = 0; i < n - 1u; i++) {
      if (i < j) {
         float tr = re[i];
         re[i]    = re[j];
         re[j]    = tr;
         float ti = im[i];
         im[i]    = im[j];
         im[j]    = ti;
      }

      uint32_t k = n >> 1;

      while (k <= j) {
         j -= k;
         k >>= 1;
      }

      j += k;
   }

   /* butterfly stages */
   for (uint32_t s = 1; s < n; s <<= 1) {
      float wR = cosf((float) (-M_PI / (double) s));
      float wI = sinf((float) (-M_PI / (double) s));

      for (uint32_t kk = 0; kk < n; kk += (s << 1)) {
         float tR = 1.0f;
         float tI = 0.0f;

         for (uint32_t jj = 0; jj < s; jj++) {
            float uR = re[kk + jj];
            float uI = im[kk + jj];
            float vR = re[kk + jj + s] * tR - im[kk + jj + s] * tI;
            float vI = re[kk + jj + s] * tI + im[kk + jj + s] * tR;

            re[kk + jj]     = uR + vR;
            im[kk + jj]     = uI + vI;
            re[kk + jj + s] = uR - vR;
            im[kk + jj + s] = uI - vI;

            float nR = tR * wR - tI * wI;
            float nI = tR * wI + tI * wR;
            tR       = nR;
            tI       = nI;
         }
      }
   }
}


void AUDIOLIB_logMelSpectrum_cn(float    *pX,
                                float    *pY,
                                float    *pWindow,
                                float    *pMelMatrix,
                                float    *pState,
                                float    *pScratch,
                                uint32_t  fftSize,
                                uint32_t  hopSize,
                                uint32_t  numChannels,
                                uint32_t  numInputSamples,
                                uint32_t  numMelBins,
                                float     power,
                                float     logOffset)
{
   uint32_t numSpectrogramBins = (fftSize / 2u) + 1u;

   /* streaming unconsumed-tail overlap geometry — must match logMelSpectrum.py */
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

   /* fft + per-frame magnitude work buffers */
   auto fftRe  = std::unique_ptr<float[]>(new float[fftSize]);
   auto fftIm  = std::unique_ptr<float[]>(new float[fftSize]);
   auto magRow = std::unique_ptr<float[]>(new float[numSpectrogramBins]);

   for (uint32_t chIdx = 0; chIdx < numChannels; chIdx++) {

      float *pInCh = &pX[chIdx * numInputSamples];

      /* streaming: prepend overlap state, then the new input */
      memcpy(pScratch, &pState[chIdx * overlapBufSize], overlapBufSize * sizeof(float));
      memcpy(&pScratch[overlapBufSize], pInCh, numInputSamples * sizeof(float));

      /* process each frame */
      for (uint32_t frameIdx = 0; frameIdx < numWindows; frameIdx++) {

         /* window and prepare fft input */
         for (uint32_t sampleIdx = 0; sampleIdx < fftSize; sampleIdx++) {
            fftRe[sampleIdx] = pScratch[(frameIdx * hopSize) + sampleIdx] * pWindow[sampleIdx];
            fftIm[sampleIdx] = 0.0f;
         }

         /* compute fft */
         fftDit(fftRe.get(), fftIm.get(), fftSize);

         /* |.|^power spectrogram over the one-sided bins */
         for (uint32_t binIdx = 0; binIdx < numSpectrogramBins; binIdx++) {
            float mag        = sqrtf((fftRe[binIdx] * fftRe[binIdx]) + (fftIm[binIdx] * fftIm[binIdx]));
            magRow[binIdx]   = powf(mag, power);
         }

         /* mel matmul + stabilised log: (bins) x (bins,mel) -> (mel) */
         for (uint32_t melIdx = 0; melIdx < numMelBins; melIdx++) {
            float acc = 0.0f;

            for (uint32_t binIdx = 0; binIdx < numSpectrogramBins; binIdx++) {
               acc += magRow[binIdx] * pMelMatrix[(binIdx * numMelBins) + melIdx];
            }

            pY[(chIdx * numWindows * numMelBins) + (frameIdx * numMelBins) + melIdx] = logf(acc + logOffset);
         }
      }

      /* streaming: save unconsumed tail into state for the next block */
      memcpy(&pState[chIdx * overlapBufSize], &pScratch[numWindows * hopSize], overlapBufSize * sizeof(float));
   }
}

/* ======================================================================== */
/*  End of file:  TISP_logMelSpectrum_cn.cpp                                */
/* ======================================================================== */
