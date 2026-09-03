// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_istft_cn.h"

#include <cmath>
#include <cstring>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/*
 * direct complex inverse DFT, valid for any length n (not only powers of two).
 *
 * computes the normalized inverse DFT: x[t] = (1/n) * sum_k X[k] * e^(+j*2*pi*t*k/n).
 * O(n^2), but this is a golden reference so correctness for arbitrary n wins over
 * speed. it replaces a radix-2 decimation-in-time version that only handled
 * power-of-two n and overran its length-n buffers for mixed-radix sizes (e.g. 320).
 *
 * re[k], im[k] : real/imag parts, length n; hold the spectrum on entry and the
 * time-domain result on exit (real part in re, imag in im).
 */
static void ifftDirect(float *re, float *im, uint32_t n)
{
   auto outRe = std::unique_ptr<float[]>(new float[n]);
   auto outIm = std::unique_ptr<float[]>(new float[n]);

   const float scale = 1.0f / (float) n;

   for (uint32_t t = 0; t < n; t++) {
      double accR = 0.0;
      double accI = 0.0;

      for (uint32_t k = 0; k < n; k++) {
         double theta = 2.0 * M_PI * (double) t * (double) k / (double) n;
         double wR    = cos(theta);
         double wI    = sin(theta);

         accR += (double)re[k] * wR - (double)im[k] * wI;
         accI += (double)re[k] * wI + (double)im[k] * wR;
      }

      outRe[t] = (float)(accR * scale);
      outIm[t] = (float)(accI * scale);
   }

   for (uint32_t t = 0; t < n; t++) {
      re[t] = outRe[t];
      im[t] = outIm[t];
   }
}


void AUDIOLIB_istft_cn(float    *pX,
                       float    *pY,
                       float    *pWindow,
                       float    *pOlaBuf,
                       uint32_t  fftSize,
                       uint32_t  hopSize,
                       uint32_t  numChannels,
                       uint32_t  numWindows,
                       bool      onesided)
{
   uint32_t    inBins           = onesided ? (fftSize + 2u) : (2u * fftSize);
   uint32_t    overlapBufSize   = fftSize - hopSize;
   uint32_t    numOutputSamples = numWindows * hopSize;
   const float normEps          = 1e-8f; /* divide-by-zero guard for the window envelope */

   /* allocate per-frame ifft work buffers */
   auto fftRe   = std::unique_ptr<float[]>(new float[fftSize]);
   auto fftIm   = std::unique_ptr<float[]>(new float[fftSize]);
   auto workBuf = std::unique_ptr<float[]>(new float[fftSize]);

   /*
    * steady-state window envelope for the torch.istft window normalization:
    *   winEnvelope[j] = sum_i pWindow[j + i*hopSize]^2   (j in [0, hopSize))
    * this is the overlap-add of pWindow^2 at output phase j -- signal-independent
    * and periodic with hopSize once frames fully overlap. each emitted sample is
    * divided by it, matching the unconditional window-envelope division that
    * torch.istft applies (it has no flag to disable). exact in steady state; the
    * first ceil(fftSize/hopSize)-1 output frames at stream start are slightly
    * under-normalized because fewer overlapping frames actually exist there.
    */
   auto winEnvelope = std::unique_ptr<float[]>(new float[hopSize]);

   for (uint32_t j = 0; j < hopSize; j++) {
      float acc = 0.0f;

      for (uint32_t idx = j; idx < fftSize; idx += hopSize) {
         acc += pWindow[idx] * pWindow[idx];
      }

      winEnvelope[j] = acc;
   }

   for (uint32_t chIdx = 0; chIdx < numChannels; chIdx++) {
      float *pOlaState = &pOlaBuf[chIdx * overlapBufSize];
      float *pOut      = &pY[chIdx * numOutputSamples];

      for (uint32_t frameIdx = 0; frameIdx < numWindows; frameIdx++) {
         uint32_t frameOffset = chIdx * numWindows * inBins + frameIdx * inBins;

         /* unpack complex interleaved input into separate re/im arrays */
         if (onesided) {
            /* input has fftSize/2+1 complex bins; reconstruct full spectrum via hermitian symmetry */
            uint32_t halfBins = fftSize / 2u + 1u;

            for (uint32_t binIdx = 0; binIdx < halfBins; binIdx++) {
               fftRe[binIdx] = pX[frameOffset + 2u * binIdx];
               fftIm[binIdx] = pX[frameOffset + 2u * binIdx + 1u];
            }

            for (uint32_t binIdx = 1u; binIdx < fftSize / 2u; binIdx++) {
               fftRe[fftSize - binIdx] = fftRe[binIdx];
               fftIm[fftSize - binIdx] = -fftIm[binIdx];
            }
         }
         else {
            /* input has fftSize complex bins */
            for (uint32_t binIdx = 0; binIdx < fftSize; binIdx++) {
               fftRe[binIdx] = pX[frameOffset + 2u * binIdx];
               fftIm[binIdx] = pX[frameOffset + 2u * binIdx + 1u];
            }
         }

         /* compute n-point inverse fft */
         ifftDirect(fftRe.get(), fftIm.get(), fftSize);

         /* apply synthesis window to real part */
         for (uint32_t sampleIdx = 0; sampleIdx < fftSize; sampleIdx++) {
            fftRe[sampleIdx] *= pWindow[sampleIdx];
         }

         /* overlap-add: load state, accumulate windowed frame, then emit + save state */
         memcpy(workBuf.get(), pOlaState, overlapBufSize * sizeof(float));
         memset(workBuf.get() + overlapBufSize, 0, hopSize * sizeof(float));

         for (uint32_t sampleIdx = 0; sampleIdx < fftSize; sampleIdx++) {
            workBuf[sampleIdx] += fftRe[sampleIdx];
         }

         /* emit hopSize samples, normalized by the steady-state window envelope */
         for (uint32_t sampleIdx = 0; sampleIdx < hopSize; sampleIdx++) {
            pOut[frameIdx * hopSize + sampleIdx] = workBuf[sampleIdx] / (winEnvelope[sampleIdx] + normEps);
         }

         /* carried OLA state stays un-normalized (raw numerator accumulator) */
         memcpy(pOlaState, workBuf.get() + hopSize, overlapBufSize * sizeof(float));
      }
   }
}
