// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

void tw_gen(float *pW, uint32_t numPoints)
{
   uint32_t     i, j, k, t;
   const double PI = 3.141592654;
   t               = numPoints >> 2;
   for (j = 1, k = 0; j <= numPoints >> 2; j = j << 2) {
      for (i = 0; i < numPoints >> 2; i += j) {
         /* TODO: Big endian requires different format of Twiddle factors? */
         pW[k]             = (float) cos(2 * PI * i / numPoints);
         pW[k + 1]         = (float) -sin(2 * PI * i / numPoints);
         pW[k + 2 * t]     = (float) cos(4 * PI * i / numPoints);
         pW[k + 2 * t + 1] = (float) -sin(4 * PI * i / numPoints);
         pW[k + 4 * t]     = (float) cos(6 * PI * i / numPoints);
         pW[k + 4 * t + 1] = (float) -sin(6 * PI * i / numPoints);
         k += 2;
      }
      k += (t) *4;
      t = t >> 2;
   }
}

void butterfly4x4(float  point0Re,
                  float  point0Im,
                  float  point1Re,
                  float  point1Im,
                  float  point2Re,
                  float  point2Im,
                  float  point3Re,
                  float  point3Im,
                  float  x1TwRe,
                  float  x1TwIm,
                  float  x2TwRe,
                  float  x2TwIm,
                  float  x3TwRe,
                  float  x3TwIm,
                  float *bflyOut)
{
   float x0Re;
   float x0Im;
   float x1Re;
   float x1Im;
   float x2Re;
   float x2Im;
   float x3Re;
   float x3Im;

   x0Re = point0Re + point1Re + point2Re + point3Re;

   x0Im = point0Im + point1Im + point2Im + point3Im;

   x1Re = (point0Re + point1Im - point2Re - point3Im) * x1TwRe - (point0Im - point1Re - point2Im + point3Re) * x1TwIm;

   x1Im = (point0Re + point1Im - point2Re - point3Im) * x1TwIm + (point0Im - point1Re - point2Im + point3Re) * x1TwRe;

   x2Re = (point0Re - point1Re + point2Re - point3Re) * x2TwRe - (point0Im - point1Im + point2Im - point3Im) * x2TwIm;

   x2Im = (point0Re - point1Re + point2Re - point3Re) * x2TwIm + (point0Im - point1Im + point2Im - point3Im) * x2TwRe;

   x3Re = (point0Re - point1Im - point2Re + point3Im) * x3TwRe - (point0Im + point1Re - point2Im - point3Re) * x3TwIm;

   x3Im = (point0Re - point1Im - point2Re + point3Im) * x3TwIm + (point0Im + point1Re - point2Im - point3Re) * x3TwRe;

   bflyOut[0] = x0Re;
   bflyOut[1] = x0Im;
   bflyOut[2] = x1Re;
   bflyOut[3] = x1Im;
   bflyOut[4] = x2Re;
   bflyOut[5] = x2Im;
   bflyOut[6] = x3Re;
   bflyOut[7] = x3Im;
}

uint32_t bitr(uint32_t in, uint32_t maxValue)
{
   uint32_t i;
   uint32_t out;
   uint32_t numBits;

   for (i = 31; i > 0; i--) {
      if ((maxValue >> i) & 1) {
         break;
      }
   }
   numBits = i + 1;

   out = 0;
   for (i = 0; i < numBits; i++) {
      out |= (((in >> i) & 1) << (numBits - 1 - i));
   }

   return (out);
}

void FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_cn(float *RESTRICT pX,
                                              float *RESTRICT pW,
                                              float *RESTRICT pY,
                                              uint32_t        numPoints,
                                              uint32_t        numChannels)
{
   float           bflyOut[4 * 2];
   uint32_t        numPointsPerDft;
   uint32_t        dftIndex;
   uint32_t        dftStartOffset;
   uint32_t        dftPointIndex;
   float *RESTRICT pWLocal;
   uint32_t        k, l;

   tw_gen(pW, numPoints);

   {
      numPointsPerDft = numPoints;
      pWLocal         = pW;
      while (numPointsPerDft > 2) {
         for (k = 0; k < numPoints / 4; k++) {
            dftIndex       = k / (numPointsPerDft / 4);
            dftStartOffset = dftIndex * numPointsPerDft;
            dftPointIndex  = k % (numPointsPerDft / 4);
            for (l = 0; l < numChannels; l++) {
               butterfly4x4(
                   pX[l * numPoints * 2 + dftStartOffset * 2 + 2 * dftPointIndex],
                   pX[l * numPoints * 2 + dftStartOffset * 2 + 2 * dftPointIndex + 1],
                   pX[l * numPoints * 2 + dftStartOffset * 2 + (numPointsPerDft / 4) * 2 + 2 * dftPointIndex],
                   pX[l * numPoints * 2 + dftStartOffset * 2 + (numPointsPerDft / 4) * 2 + 2 * dftPointIndex + 1],
                   pX[l * numPoints * 2 + dftStartOffset * 2 + (numPointsPerDft / 2) * 2 + 2 * dftPointIndex],
                   pX[l * numPoints * 2 + dftStartOffset * 2 + (numPointsPerDft / 2) * 2 + 2 * dftPointIndex + 1],
                   pX[l * numPoints * 2 + dftStartOffset * 2 + 3 * (numPointsPerDft / 4) * 2 + 2 * dftPointIndex],
                   pX[l * numPoints * 2 + dftStartOffset * 2 + 3 * (numPointsPerDft / 4) * 2 + 2 * dftPointIndex + 1],
                   pWLocal[dftPointIndex * 2],                                 /* re of twiddle for x_1 */
                   pWLocal[dftPointIndex * 2 + 1],                             /* im of twiddle for x_1 */
                   pWLocal[(numPointsPerDft / 4) * 2 + dftPointIndex * 2],     /* re of twiddle for x_2 */
                   pWLocal[(numPointsPerDft / 4) * 2 + dftPointIndex * 2 + 1], /* re of twiddle for x_2 */
                   pWLocal[(numPointsPerDft / 2) * 2 + dftPointIndex * 2],     /* re of twiddle for x_2 */
                   pWLocal[(numPointsPerDft / 2) * 2 + dftPointIndex * 2 + 1], /* re of twiddle for x_2 */
                   bflyOut);

               pX[l * numPoints * 2 + dftStartOffset * 2 + 2 * dftPointIndex]                             = bflyOut[0];
               pX[l * numPoints * 2 + dftStartOffset * 2 + 2 * dftPointIndex + 1]                         = bflyOut[1];
               pX[l * numPoints * 2 + dftStartOffset * 2 + (numPointsPerDft / 4) * 2 + 2 * dftPointIndex] = bflyOut[4];
               pX[l * numPoints * 2 + dftStartOffset * 2 + (numPointsPerDft / 4) * 2 + 2 * dftPointIndex + 1] =
                   bflyOut[5];
               pX[l * numPoints * 2 + dftStartOffset * 2 + (numPointsPerDft / 2) * 2 + 2 * dftPointIndex] = bflyOut[2];
               pX[l * numPoints * 2 + dftStartOffset * 2 + (numPointsPerDft / 2) * 2 + 2 * dftPointIndex + 1] =
                   bflyOut[3];
               pX[l * numPoints * 2 + dftStartOffset * 2 + 3 * (numPointsPerDft / 4) * 2 + 2 * dftPointIndex] =
                   bflyOut[6];
               pX[l * numPoints * 2 + dftStartOffset * 2 + 3 * (numPointsPerDft / 4) * 2 + 2 * dftPointIndex + 1] =
                   bflyOut[7];
            }
         }
         numPointsPerDft /= 4;
         pWLocal += 3 * numPointsPerDft * 2;
      }

      if (numPointsPerDft == 2) {
         for (k = 0; k < numPoints / 2; k++) {
            for (l = 0; l < numChannels; l++) {
               bflyOut[0] = pX[l * numPoints * 2 + 2 * k * 2] + pX[l * numPoints * 2 + (2 * k + 1) * 2];
               bflyOut[1] = pX[l * numPoints * 2 + 2 * k * 2 + 1] + pX[l * numPoints * 2 + (2 * k + 1) * 2 + 1];
               bflyOut[2] = pX[l * numPoints * 2 + 2 * k * 2] - pX[l * numPoints * 2 + (2 * k + 1) * 2];
               bflyOut[3] = pX[l * numPoints * 2 + 2 * k * 2 + 1] - pX[l * numPoints * 2 + (2 * k + 1) * 2 + 1];

               pX[l * numPoints * 2 + 2 * k * 2]           = bflyOut[0];
               pX[l * numPoints * 2 + 2 * k * 2 + 1]       = bflyOut[1];
               pX[l * numPoints * 2 + (2 * k + 1) * 2]     = bflyOut[2];
               pX[l * numPoints * 2 + (2 * k + 1) * 2 + 1] = bflyOut[3];
            }
         }
      }

      /* Do swap of points between bit-reversed locations */
      for (k = 0; k < numPoints; k++) {
         uint32_t kBitr;

         kBitr = bitr(k, numPoints - 1);
         for (l = 0; l < numChannels; l++) {
            pY[l * numPoints * 2 + 2 * k]     = pX[l * numPoints * 2 + 2 * kBitr];
            pY[l * numPoints * 2 + 2 * k + 1] = pX[l * numPoints * 2 + 2 * kBitr + 1];
         }
      }
   }

   return;
}

void TISP_blockCopyTranspose_cn(double *RESTRICT pX,
                                double *RESTRICT pY,
                                size_t           width,
                                size_t           height,
                                size_t           inPitch,
                                size_t           outPitch)
{
   for (uint32_t i = 0; i < width; i++) {
      for (uint32_t j = 0; j < height; j++) {
         pY[i * outPitch + j] = pX[j * inPitch + i];
      }
   }
}

void TISP_fft2d_test_cn(float *pX, float *pW, float *pTemp, float *pY, size_t numPoints, size_t numChannels)
{

   FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_cn(pX, pW, pTemp, numPoints, numChannels);

   TISP_blockCopyTranspose_cn((double *) pTemp, (double *) pX, numPoints, numChannels, numPoints, numChannels);

   FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_cn(pX, pW, pY, numChannels, numPoints);
   return;
}
