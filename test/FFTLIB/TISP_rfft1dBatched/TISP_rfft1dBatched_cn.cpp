// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_rfft1dBatched_cn.h"
#include <fftlib.h>
#include <math.h>
#include <stdint.h>

/*DFT twiddles */
#define COS_TWO_PI_DIV_3 0.5f                  /* cos(2π/3) */
#define SIN_TWO_PI_DIV_3 0.86602540378443871f  /* sin(2π/3) */
#define COS_TWO_PI_DIV_5 0.30901699437494745f  /* cos(2π/5) */
#define SIN_TWO_PI_DIV_5 0.95105651629515353f  /* sin(2π/5) */
#define COS_FOUR_PI_DIV_5 0.80901699437494734f /* cos(4π/5) */
#define SIN_FOUR_PI_DIV_5 0.58778525229247325f /* sin(4π/5) */

static void butterfly4x4(FFTLIB_F32  point0Re,
                         FFTLIB_F32  point0Im,
                         FFTLIB_F32  point1Re,
                         FFTLIB_F32  point1Im,
                         FFTLIB_F32  point2Re,
                         FFTLIB_F32  point2Im,
                         FFTLIB_F32  point3Re,
                         FFTLIB_F32  point3Im,
                         FFTLIB_F32  x1TwRe,
                         FFTLIB_F32  x1TwIm,
                         FFTLIB_F32  x2TwRe,
                         FFTLIB_F32  x2TwIm,
                         FFTLIB_F32  x3TwRe,
                         FFTLIB_F32  x3TwIm,
                         FFTLIB_F32 *bflyOut)
{
   FFTLIB_F32 x0Re;
   FFTLIB_F32 x0Im;
   FFTLIB_F32 x1Re;
   FFTLIB_F32 x1Im;
   FFTLIB_F32 x2Re;
   FFTLIB_F32 x2Im;
   FFTLIB_F32 x3Re;
   FFTLIB_F32 x3Im;

   x0Re = point0Re + point1Re + point2Re + point3Re;

   x0Im = point0Im + point1Im + point2Im + point3Im;

   x1Re =
       ((point0Re + point1Im - point2Re - point3Im) * x1TwRe) - ((point0Im - point1Re - point2Im + point3Re) * x1TwIm);

   x1Im =
       ((point0Re + point1Im - point2Re - point3Im) * x1TwIm) + ((point0Im - point1Re - point2Im + point3Re) * x1TwRe);

   x2Re =
       ((point0Re - point1Re + point2Re - point3Re) * x2TwRe) - ((point0Im - point1Im + point2Im - point3Im) * x2TwIm);

   x2Im =
       ((point0Re - point1Re + point2Re - point3Re) * x2TwIm) + ((point0Im - point1Im + point2Im - point3Im) * x2TwRe);

   x3Re =
       ((point0Re - point1Im - point2Re + point3Im) * x3TwRe) - ((point0Im + point1Re - point2Im - point3Re) * x3TwIm);

   x3Im =
       ((point0Re - point1Im - point2Re + point3Im) * x3TwIm) + ((point0Im + point1Re - point2Im - point3Re) * x3TwRe);

   bflyOut[0] = x0Re;
   bflyOut[1] = x0Im;
   bflyOut[2] = x1Re;
   bflyOut[3] = x1Im;
   bflyOut[4] = x2Re;
   bflyOut[5] = x2Im;
   bflyOut[6] = x3Re;
   bflyOut[7] = x3Im;
}

static void butterfly5x5(FFTLIB_F32  p0Re,
                         FFTLIB_F32  p0Im,
                         FFTLIB_F32  p1Re,
                         FFTLIB_F32  p1Im,
                         FFTLIB_F32  p2Re,
                         FFTLIB_F32  p2Im,
                         FFTLIB_F32  p3Re,
                         FFTLIB_F32  p3Im,
                         FFTLIB_F32  p4Re,
                         FFTLIB_F32  p4Im,
                         FFTLIB_F32  w1Re,
                         FFTLIB_F32  w1Im,
                         FFTLIB_F32  w2Re,
                         FFTLIB_F32  w2Im,
                         FFTLIB_F32  w3Re,
                         FFTLIB_F32  w3Im,
                         FFTLIB_F32  w4Re,
                         FFTLIB_F32  w4Im,
                         FFTLIB_F32 *bflyOut)
{
   FFTLIB_F32 XbaseRe[5], XbaseIm[5];

   XbaseRe[0] = p0Re + p1Re + p2Re + p3Re + p4Re;
   XbaseIm[0] = p0Im + p1Im + p2Im + p3Im + p4Im;

   XbaseRe[1] = p0Re + p1Re * COS_TWO_PI_DIV_5 + p1Im * SIN_TWO_PI_DIV_5 + p2Re * -COS_FOUR_PI_DIV_5 +
                p2Im * SIN_FOUR_PI_DIV_5 + p3Re * -COS_FOUR_PI_DIV_5 + p3Im * -SIN_FOUR_PI_DIV_5 +
                p4Re * COS_TWO_PI_DIV_5 + p4Im * -SIN_TWO_PI_DIV_5;
   XbaseIm[1] = p0Im + p1Re * -SIN_TWO_PI_DIV_5 + p1Im * COS_TWO_PI_DIV_5 + p2Re * -SIN_FOUR_PI_DIV_5 +
                p2Im * -COS_FOUR_PI_DIV_5 + p3Re * SIN_FOUR_PI_DIV_5 + p3Im * -COS_FOUR_PI_DIV_5 +
                p4Re * SIN_TWO_PI_DIV_5 + p4Im * COS_TWO_PI_DIV_5;

   XbaseRe[2] = p0Re + p1Re * -COS_FOUR_PI_DIV_5 + p1Im * SIN_FOUR_PI_DIV_5 + p2Re * COS_TWO_PI_DIV_5 +
                p2Im * -SIN_TWO_PI_DIV_5 + p3Re * COS_TWO_PI_DIV_5 + p3Im * SIN_TWO_PI_DIV_5 +
                p4Re * -COS_FOUR_PI_DIV_5 + p4Im * -SIN_FOUR_PI_DIV_5;
   XbaseIm[2] = p0Im + p1Re * -SIN_FOUR_PI_DIV_5 + p1Im * -COS_FOUR_PI_DIV_5 + p2Re * SIN_TWO_PI_DIV_5 +
                p2Im * COS_TWO_PI_DIV_5 + p3Re * -SIN_TWO_PI_DIV_5 + p3Im * COS_TWO_PI_DIV_5 +
                p4Re * SIN_FOUR_PI_DIV_5 + p4Im * -COS_FOUR_PI_DIV_5;

   XbaseRe[3] = p0Re + p1Re * -COS_FOUR_PI_DIV_5 + p1Im * -SIN_FOUR_PI_DIV_5 + p2Re * COS_TWO_PI_DIV_5 +
                p2Im * SIN_TWO_PI_DIV_5 + p3Re * COS_TWO_PI_DIV_5 + p3Im * -SIN_TWO_PI_DIV_5 +
                p4Re * -COS_FOUR_PI_DIV_5 + p4Im * SIN_FOUR_PI_DIV_5;
   XbaseIm[3] = p0Im + p1Re * SIN_FOUR_PI_DIV_5 + p1Im * -COS_FOUR_PI_DIV_5 + p2Re * -SIN_TWO_PI_DIV_5 +
                p2Im * COS_TWO_PI_DIV_5 + p3Re * SIN_TWO_PI_DIV_5 + p3Im * COS_TWO_PI_DIV_5 +
                p4Re * -SIN_FOUR_PI_DIV_5 + p4Im * -COS_FOUR_PI_DIV_5;

   XbaseRe[4] = p0Re + p1Re * COS_TWO_PI_DIV_5 + p1Im * -SIN_TWO_PI_DIV_5 + p2Re * -COS_FOUR_PI_DIV_5 +
                p2Im * -SIN_FOUR_PI_DIV_5 + p3Re * -COS_FOUR_PI_DIV_5 + p3Im * SIN_FOUR_PI_DIV_5 +
                p4Re * COS_TWO_PI_DIV_5 + p4Im * SIN_TWO_PI_DIV_5;
   XbaseIm[4] = p0Im + p1Re * SIN_TWO_PI_DIV_5 + p1Im * COS_TWO_PI_DIV_5 + p2Re * SIN_FOUR_PI_DIV_5 +
                p2Im * -COS_FOUR_PI_DIV_5 + p3Re * -SIN_FOUR_PI_DIV_5 + p3Im * -COS_FOUR_PI_DIV_5 +
                p4Re * -SIN_TWO_PI_DIV_5 + p4Im * COS_TWO_PI_DIV_5;

   bflyOut[0] = XbaseRe[0];
   bflyOut[1] = XbaseIm[0];
   bflyOut[2] = XbaseRe[1] * w1Re - XbaseIm[1] * w1Im;
   bflyOut[3] = XbaseRe[1] * w1Im + XbaseIm[1] * w1Re;
   bflyOut[4] = XbaseRe[2] * w2Re - XbaseIm[2] * w2Im;
   bflyOut[5] = XbaseRe[2] * w2Im + XbaseIm[2] * w2Re;
   bflyOut[6] = XbaseRe[3] * w3Re - XbaseIm[3] * w3Im;
   bflyOut[7] = XbaseRe[3] * w3Im + XbaseIm[3] * w3Re;
   bflyOut[8] = XbaseRe[4] * w4Re - XbaseIm[4] * w4Im;
   bflyOut[9] = XbaseRe[4] * w4Im + XbaseIm[4] * w4Re;
}

static void butterfly3x3(FFTLIB_F32  x0Re,
                         FFTLIB_F32  x0Im,
                         FFTLIB_F32  x1Re,
                         FFTLIB_F32  x1Im,
                         FFTLIB_F32  x2Re,
                         FFTLIB_F32  x2Im,
                         FFTLIB_F32  w1Re,
                         FFTLIB_F32  w1Im,
                         FFTLIB_F32  w2Re,
                         FFTLIB_F32  w2Im,
                         FFTLIB_F32 *bflyOut)
{
   FFTLIB_F32 XbaseRe[3], XbaseIm[3];

   XbaseRe[0] = x0Re + x1Re + x2Re;
   XbaseIm[0] = x0Im + x1Im + x2Im;

   XbaseRe[1] =
       x0Re - x1Re * COS_TWO_PI_DIV_3 + x1Im * SIN_TWO_PI_DIV_3 - x2Re * COS_TWO_PI_DIV_3 - x2Im * SIN_TWO_PI_DIV_3;
   XbaseIm[1] =
       x0Im - x1Re * SIN_TWO_PI_DIV_3 - x1Im * COS_TWO_PI_DIV_3 + x2Re * SIN_TWO_PI_DIV_3 - x2Im * COS_TWO_PI_DIV_3;

   XbaseRe[2] =
       x0Re - x1Re * COS_TWO_PI_DIV_3 - x1Im * SIN_TWO_PI_DIV_3 - x2Re * COS_TWO_PI_DIV_3 + x2Im * SIN_TWO_PI_DIV_3;
   XbaseIm[2] =
       x0Im + x1Re * SIN_TWO_PI_DIV_3 - x1Im * COS_TWO_PI_DIV_3 - x2Re * SIN_TWO_PI_DIV_3 - x2Im * COS_TWO_PI_DIV_3;

   bflyOut[0] = XbaseRe[0];
   bflyOut[1] = XbaseIm[0];
   bflyOut[2] = XbaseRe[1] * w1Re - XbaseIm[1] * w1Im;
   bflyOut[3] = XbaseRe[1] * w1Im + XbaseIm[1] * w1Re;
   bflyOut[4] = XbaseRe[2] * w2Re - XbaseIm[2] * w2Im;
   bflyOut[5] = XbaseRe[2] * w2Im + XbaseIm[2] * w2Re;
}

static uint32_t bitr(uint32_t in, uint32_t maxValue)
{
   uint32_t i;
   uint32_t out;
   uint32_t numBits;

   for (i = 31; i > 0U; i--) {
      if (((maxValue >> i) & 1U) != 0U) {
         break;
      }
   }
   numBits = i + 1U;

   out = 0;
   for (i = 0; i < numBits; i++) {
      out |= (((in >> i) & 1U) << (numBits - 1U - i));
   }

   return (out);
}

static FFTLIB_STATUS TISP_c2cFft_cn(FFTLIB_F32           *pX,
                                    FFTLIB_bufParams1D_t *bufParamsX,
                                    FFTLIB_F32           *pW,
                                    FFTLIB_bufParams1D_t *bufParamsW,
                                    FFTLIB_F32           *pY,
                                    FFTLIB_bufParams1D_t *bufParamsY,
                                    uint32_t              numPoints,
                                    uint32_t              numChannels)
{
   FFTLIB_STATUS status = FFTLIB_SUCCESS;
   FFTLIB_F32    bflyOut[5 * 2];
   uint32_t      numPointsPerDft;
   uint32_t      dftIndex;
   uint32_t      dftStartOffset;
   uint32_t      dftPointIndex;
   FFTLIB_F32   *pWLocal;
   uint32_t      k, l;
   uint32_t      r5, r3, r4, r2;
   uint32_t      numRadix5Stages = 0;
   uint32_t      numRadix3Stages = 0;
   uint32_t      numRadix4Stages = 0;
   uint32_t      numRadix2Stages = 0;
   uint32_t      stageTemp       = numPoints;

#ifdef FFTLIB_CHECK_PARAMS
   status = FFTLIB_fft1dBatched_i32fc_c32fc_o32fc_checkParams_cn(pX, bufParamsX, pW, bufParamsW, pY, bufParamsY,
                                                                 numPoints, numChannels);
   if (status == FFTLIB_SUCCESS)
#endif
   {
      while (stageTemp > 1U) {
         if (stageTemp % 5U == 0U) {
            numRadix5Stages++;
            stageTemp /= 5U;
         }
         else if (stageTemp % 3U == 0U) {
            numRadix3Stages++;
            stageTemp /= 3U;
         }
         else if (stageTemp % 4U == 0U) {
            numRadix4Stages++;
            stageTemp /= 4U;
         }
         else if (stageTemp % 2U == 0U) {
            numRadix2Stages++;
            stageTemp /= 2U;
         }
         else {
            break;
         }
      }

      numPointsPerDft = numPoints;
      pWLocal         = pW;

      /* Execute Radix-5 stages */
      for (r5 = 0; r5 < numRadix5Stages; r5++) {
         for (k = 0; k < (numPoints / 5U); k++) {
            dftIndex       = k / (numPointsPerDft / 5U);
            dftStartOffset = dftIndex * numPointsPerDft;
            dftPointIndex  = k % (numPointsPerDft / 5U);
            for (l = 0; l < numChannels; l++) {
               butterfly5x5(pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex)],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex) + 1U],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 5U) * 2U) +
                               (2U * dftPointIndex)],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 5U) * 2U) +
                               (2U * dftPointIndex) + 1U],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * (numPointsPerDft / 5U) * 2U) +
                               (2U * dftPointIndex)],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * (numPointsPerDft / 5U) * 2U) +
                               (2U * dftPointIndex) + 1U],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (3U * (numPointsPerDft / 5U) * 2U) +
                               (2U * dftPointIndex)],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (3U * (numPointsPerDft / 5U) * 2U) +
                               (2U * dftPointIndex) + 1U],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (4U * (numPointsPerDft / 5U) * 2U) +
                               (2U * dftPointIndex)],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (4U * (numPointsPerDft / 5U) * 2U) +
                               (2U * dftPointIndex) + 1U],
                            pWLocal[dftPointIndex * 2U], pWLocal[(dftPointIndex * 2U) + 1U],
                            pWLocal[((numPointsPerDft / 5U) * 2U) + (dftPointIndex * 2U)],
                            pWLocal[((numPointsPerDft / 5U) * 2U) + (dftPointIndex * 2U) + 1U],
                            pWLocal[(2U * (numPointsPerDft / 5U) * 2U) + (dftPointIndex * 2U)],
                            pWLocal[(2U * (numPointsPerDft / 5U) * 2U) + (dftPointIndex * 2U) + 1U],
                            pWLocal[(3U * (numPointsPerDft / 5U) * 2U) + (dftPointIndex * 2U)],
                            pWLocal[(3U * (numPointsPerDft / 5U) * 2U) + (dftPointIndex * 2U) + 1U], bflyOut);
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex)]      = bflyOut[0];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex) + 1U] = bflyOut[1];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 5U) * 2U) + (2U * dftPointIndex)] =
                   bflyOut[2];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 5U) * 2U) + (2U * dftPointIndex) +
                  1U]                        = bflyOut[3];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * (numPointsPerDft / 5U) * 2U) +
                  (2U * dftPointIndex)]      = bflyOut[4];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * (numPointsPerDft / 5U) * 2U) +
                  (2U * dftPointIndex) + 1U] = bflyOut[5];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (3U * (numPointsPerDft / 5U) * 2U) +
                  (2U * dftPointIndex)]      = bflyOut[6];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (3U * (numPointsPerDft / 5U) * 2U) +
                  (2U * dftPointIndex) + 1U] = bflyOut[7];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (4U * (numPointsPerDft / 5U) * 2U) +
                  (2U * dftPointIndex)]      = bflyOut[8];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (4U * (numPointsPerDft / 5U) * 2U) +
                  (2U * dftPointIndex) + 1U] = bflyOut[9];
            }
         }
         numPointsPerDft /= 5U;
         pWLocal += 4U * numPointsPerDft * 2U;
      }
      /* Execute Radix-3 stages */
      for (r3 = 0; r3 < numRadix3Stages; r3++) {
         for (k = 0; k < (numPoints / 3U); k++) {
            dftIndex       = k / (numPointsPerDft / 3U);
            dftStartOffset = dftIndex * numPointsPerDft;
            dftPointIndex  = k % (numPointsPerDft / 3U);
            for (l = 0; l < numChannels; l++) {
               butterfly3x3(pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex)],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex) + 1U],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 3U) * 2U) +
                               (2U * dftPointIndex)],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 3U) * 2U) +
                               (2U * dftPointIndex) + 1U],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * (numPointsPerDft / 3U) * 2U) +
                               (2U * dftPointIndex)],
                            pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * (numPointsPerDft / 3U) * 2U) +
                               (2U * dftPointIndex) + 1U],
                            pWLocal[dftPointIndex * 2U], pWLocal[(dftPointIndex * 2U) + 1U],
                            pWLocal[((numPointsPerDft / 3U) * 2U) + (dftPointIndex * 2U)],
                            pWLocal[((numPointsPerDft / 3U) * 2U) + (dftPointIndex * 2U) + 1U], bflyOut);
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex)]      = bflyOut[0];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex) + 1U] = bflyOut[1];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 3U) * 2U) + (2U * dftPointIndex)] =
                   bflyOut[2];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 3U) * 2U) + (2U * dftPointIndex) +
                  1U]                        = bflyOut[3];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * (numPointsPerDft / 3U) * 2U) +
                  (2U * dftPointIndex)]      = bflyOut[4];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * (numPointsPerDft / 3U) * 2U) +
                  (2U * dftPointIndex) + 1U] = bflyOut[5];
            }
         }
         numPointsPerDft /= 3U;
         pWLocal += 2U * numPointsPerDft * 2U;
      }

      /* Execute Radix-4 stages */
      for (r4 = 0; r4 < numRadix4Stages; r4++) {
         for (k = 0; k < (numPoints / 4U); k++) {
            dftIndex       = k / (numPointsPerDft / 4U);
            dftStartOffset = dftIndex * numPointsPerDft;
            dftPointIndex  = k % (numPointsPerDft / 4U);
            for (l = 0; l < numChannels; l++) {
               butterfly4x4(
                   pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex)],
                   pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex) + 1],
                   pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 4U) * 2U) +
                      (2U * dftPointIndex)],
                   pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 4U) * 2U) +
                      (2U * dftPointIndex) + 1U],
                   pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 2U) * 2U) +
                      (2U * dftPointIndex)],
                   pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 2U) * 2U) +
                      (2U * dftPointIndex) + 1U],
                   pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((3U * (numPointsPerDft / 4U)) * 2U) +
                      (2U * dftPointIndex)],
                   pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((3U * (numPointsPerDft / 4U)) * 2U) +
                      (2U * dftPointIndex) + 1U],
                   pWLocal[dftPointIndex * 2U],                                        /* re of twiddle for x_1 */
                   pWLocal[(dftPointIndex * 2U) + 1U],                                 /* im of twiddle for x_1 */
                   pWLocal[((numPointsPerDft / 4U) * 2U) + (dftPointIndex * 2U)],      /* re of twiddle for x_2 */
                   pWLocal[((numPointsPerDft / 4U) * 2U) + (dftPointIndex * 2U) + 1U], /* re of twiddle for x_2 */
                   pWLocal[((numPointsPerDft / 2U) * 2U) + (dftPointIndex * 2U)],      /* re of twiddle for x_2 */
                   pWLocal[((numPointsPerDft / 2U) * 2U) + (dftPointIndex * 2U) + 1U], /* re of twiddle for x_2 */
                   bflyOut);

               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex)]     = bflyOut[0];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (2U * dftPointIndex) + 1] = bflyOut[1];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 4U) * 2U) + (2U * dftPointIndex)] =
                   bflyOut[4];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 4U) * 2U) + (2U * dftPointIndex) +
                  1U] = bflyOut[5];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 2U) * 2U) + (2U * dftPointIndex)] =
                   bflyOut[2];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + ((numPointsPerDft / 2U) * 2U) + (2U * dftPointIndex) +
                  1U]                        = bflyOut[3];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (3U * (numPointsPerDft / 4U) * 2U) +
                  (2U * dftPointIndex)]      = bflyOut[6];
               pX[(l * numPoints * 2U) + (dftStartOffset * 2U) + (3 * (numPointsPerDft / 4U) * 2U) +
                  (2U * dftPointIndex) + 1U] = bflyOut[7];
            }
         }
         numPointsPerDft /= 4U;
         pWLocal += 3U * numPointsPerDft * 2U;
      }

      /* Execute Radix-2 stages */
      for (r2 = 0; r2 < numRadix2Stages; r2++) {
         for (k = 0; k < (numPoints / 2U); k++) {
            for (l = 0; l < numChannels; l++) {
               bflyOut[0] =
                   pX[(l * numPoints * 2U) + (2U * k * 2U)] + pX[(l * numPoints * 2U) + (((2U * k) + 1U) * 2U)];
               bflyOut[1] = pX[(l * numPoints * 2U) + (2U * k * 2U) + 1U] +
                            pX[(l * numPoints * 2U) + (((2U * k) + 1U) * 2U) + 1U];
               bflyOut[2] =
                   pX[(l * numPoints * 2U) + (2U * k * 2U)] - pX[(l * numPoints * 2U) + (((2U * k) + 1U) * 2U)];
               bflyOut[3]                                   = pX[(l * numPoints * 2U) + (2U * k * 2U) + 1U] -
                                                              pX[(l * numPoints * 2U) + (((2U * k) + 1U) * 2U) + 1U];
               pX[(l * numPoints * 2U) + (2U * k * 2U)]     = bflyOut[0];
               pX[(l * numPoints * 2U) + (2U * k * 2U) + 1] = bflyOut[1];
               pX[(l * numPoints * 2U) + (((2U * k) + 1U) * 2U)]      = bflyOut[2];
               pX[(l * numPoints * 2U) + (((2U * k) + 1U) * 2U) + 1U] = bflyOut[3];
            }
         }
         numPointsPerDft /= 2U;
      }

      /* Do swap of points between bit-reversed locations */

      /* check if the numPoints is only power of 2 */
      uint32_t isPowerOf2 = (numRadix3Stages == 0 && numRadix5Stages == 0) ? 1U : 0U;

      if (isPowerOf2) {
         /* use bit-reversal */
         for (k = 0; k < numPoints; k++) {
            uint32_t kBitr;
            kBitr = bitr(k, numPoints - 1U);
            for (l = 0; l < numChannels; l++) {
               pY[(l * numPoints * 2U) + (2U * k)]      = pX[(l * numPoints * 2U) + (2U * kBitr)];
               pY[(l * numPoints * 2U) + (2U * k) + 1U] = pX[(l * numPoints * 2U) + (2U * kBitr) + 1U];
            }
         }
      }
      else {
         /* use digit-reversal */
         uint32_t radixSeq[32], numRadixStages = 0U;
         uint32_t tempCheck = numPoints;

         /* Radix-5 */
         while (tempCheck % 5U == 0U) {
            radixSeq[numRadixStages++] = 5U;
            tempCheck /= 5U;
         }
         /* Radix-3 */
         while (tempCheck % 3U == 0U) {
            radixSeq[numRadixStages++] = 3U;
            tempCheck /= 3U;
         }
         /* Radix-4 */
         while (tempCheck % 4U == 0U) {
            radixSeq[numRadixStages++] = 2U;
            radixSeq[numRadixStages++] = 2U;
            tempCheck /= 4U;
         }
         /* Radix-2 */
         if (tempCheck == 2U) {
            radixSeq[numRadixStages++] = 2U;
            tempCheck /= 2U;
         }

         /* Apply mixed-radix digit-reversal permutation */
         for (k = 0U; k < numPoints; k++) {
            /* Extract digits of k in the radix sequence */
            uint32_t kTemp = k;
            uint32_t digits[32];
            uint32_t s;

            for (s = 0U; s < numRadixStages; s++) {
               digits[s] = kTemp % radixSeq[s];
               kTemp /= radixSeq[s];
            }

            /* Recompose in reversed stage order */
            uint32_t kBitr = 0U;
            uint32_t mult  = 1U;
            int32_t  sRev;

            for (sRev = numRadixStages - 1; sRev >= 0; sRev--) {
               kBitr += digits[sRev] * mult;
               mult *= radixSeq[sRev];
            }

            /* Copy from reversed input position to natural output position */
            for (l = 0U; l < numChannels; l++) {
               pY[(l * numPoints * 2U) + (2U * k)]      = pX[(l * numPoints * 2U) + (2U * kBitr)];
               pY[(l * numPoints * 2U) + (2U * k) + 1U] = pX[(l * numPoints * 2U) + (2U * kBitr) + 1U];
            }
         }
      }
   }

   return (status);
}

static FFTLIB_STATUS TISP_r2cFft_cn(FFTLIB_F32           *pX,
                                    FFTLIB_bufParams1D_t *bufParamsX,
                                    FFTLIB_F32           *pW,
                                    FFTLIB_bufParams1D_t *bufParamsW,
                                    FFTLIB_F32           *pXFFT,
                                    FFTLIB_bufParams1D_t *bufParamsXFFT,
                                    FFTLIB_F32           *pSf,
                                    FFTLIB_bufParams1D_t *bufParamsSf,
                                    FFTLIB_F32           *pY,
                                    FFTLIB_bufParams1D_t *bufParamsY,
                                    uint32_t              numPoints,
                                    uint32_t              numChannels,
                                    uint8_t               outmode)
{
   FFTLIB_STATUS        status = FFTLIB_SUCCESS;
   FFTLIB_bufParams1D_t bufParamsX_FFT;

   /* Outmode: number of complex out points */
   /* numPointsOut = N/2 + 1 for outmode = 0 but for calculation purposes keep it at N/2 */
   uint32_t numPointsOut = (outmode == 1U) ? numPoints : ((numPoints >> 1U) + 1U);

   bufParamsX_FFT.dim_x = numPoints;

   /* Do N/2 batched complex fft with N real numbers*/
   TISP_c2cFft_cn(pX, &bufParamsX_FFT, pW, &bufParamsX_FFT, pXFFT, &bufParamsX_FFT, numPoints >> 1, numChannels);

   uint32_t ch;
   /* Per-channel post-processing: input offset c = ch * numPoints floats (N/2 complex),
      output offset cout = ch * numPointsOut complex points */
   for (ch = 0; ch < numChannels; ch++) {
      uint32_t i    = 0;
      uint32_t c    = 0;
      uint32_t cout = 0;

      FFTLIB_F32 temp0Real;
      FFTLIB_F32 temp0Imag;
      FFTLIB_F32 temp1Real;
      FFTLIB_F32 temp1Imag;

      FFTLIB_F32 var0Real;
      FFTLIB_F32 var0Imag;
      FFTLIB_F32 var1Real;
      FFTLIB_F32 var1Imag;

      c    = (ch * (numPoints));
      cout = (ch * (numPointsOut));

      for (i = 0; i < (numPoints >> 1U); i += 1U) {
         var0Real = pXFFT[c + (2U * i)]; // [1]
         var0Imag = pXFFT[c + (2U * i) + 1U];
         if (!i) {
            var1Real = 0;
            var1Imag = 0;
         }
         else {
            var1Real = pXFFT[c + (numPoints) - (2U * i)];
            var1Imag = pXFFT[c + (numPoints) - (2U * i) + 1U];
         }

         temp0Real = ((var0Real * pSf[2U * i]) - (var0Imag * pSf[(2U * i) + 1U]));
         temp0Imag = ((var0Real * pSf[(2U * i) + 1U]) + (var0Imag * pSf[2U * i]));

         if (!i) {
            temp1Real = ((pXFFT[c + 0U] * pSf[(2U * numPoints) - (2U * i) - 2U]) +
                         (pXFFT[c + 1U] * pSf[(2U * numPoints) - (2U * i) - 2U + 1U]));
            temp1Imag = ((pXFFT[c + 0U] * pSf[(2U * numPoints) - (2U * i) - 2U]) -
                         (pXFFT[c + 1U] * pSf[(2U * numPoints) - (2U * i) - 2U + 1U]));
         }
         else {
            temp1Real = ((var1Real * pSf[(2U * numPoints) - (2U * i) - 2U]) +
                         (var1Imag * pSf[(2U * numPoints) - (2U * i) - 2U + 1U]));
            temp1Imag = ((var1Real * pSf[(2U * numPoints) - (2U * i) - 2U + 1U]) -
                         (var1Imag * pSf[(2U * numPoints) - (2U * i) - 2U]));
         }

         /* i < N/2 <= numPointsOut, so this write is always within the channel's output buffer */
         pY[(cout << 1U) + (2U * i)]      = temp0Real + temp1Real;
         pY[(cout << 1U) + (2U * i) + 1U] = temp0Imag + temp1Imag;

         if (outmode == 0) {
            continue;
         }
         /* if half spectrum, no need to check complex conjugate boundaries */

         /* The DC bin (i = 0) has no conjugate mirror: its mirror index would be numPointsOut,
            one past the end of the channel's output. Mirror only i >= 1 (indices N-1 .. N/2+1). */
         if (i != 0U) {
            pY[(cout << 1U) + ((numPointsOut) * 2U) - (2U * (i))]      = pY[(cout << 1U) + (2U * i)];
            pY[(cout << 1U) + ((numPointsOut) * 2U) - (2U * (i)) + 1U] = pY[(cout << 1U) + (2U * i) + 1U] * (-1.0f);
         }
      } // end numPoints loop

      /* calculate nyquist frequency bin */
      // similarly for i = numPoints >> 1
      i = (numPoints >> 1U);

      pY[(cout << 1U) + (2U * i)]      = pXFFT[c + 0U] - pXFFT[c + 1U];
      pY[(cout << 1U) + (2U * i) + 1U] = 0;

   } // end numChannels loop

   return (status);
}

/* twiddles and split factors: tw_gen_r2c / split_factor_gen_r2c, public FFTLIB API declared in fftlib.h */

/* Natural C of the TISP RFFT1dBatched node. pX is overwritten (it is the working buffer of the complex FFT).
 * pW and pSf are scratch for the twiddles (>= 4 * numPoints floats) and split factors (>= 2 * numPoints floats),
 * pXFFT is scratch for the intermediate complex FFT (numPoints * numChannels floats). */
void TISP_rfft1dBatched_cn(float   *pX,
                           float   *pW,
                           float   *pSf,
                           float   *pXFFT,
                           float   *pY,
                           uint32_t numPoints,
                           uint32_t numChannels,
                           bool     fullSpectrum)
{
   FFTLIB_bufParams1D_t bufParams;
   bufParams.dim_x     = numPoints * numChannels;
   bufParams.data_type = FFTLIB_FLOAT32;

   tw_gen_r2c(pW, (int32_t) numPoints);
   split_factor_gen_r2c(pSf, (int32_t) numPoints);

   TISP_r2cFft_cn(pX, &bufParams, pW, &bufParams, pXFFT, &bufParams, pSf, &bufParams, pY, &bufParams, numPoints,
                  numChannels, fullSpectrum ? 1U : 0U);
}
