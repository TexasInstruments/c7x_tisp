// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: Apache-2.0

#include <audiolib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <tisp.hpp>

#include "TISP_logMelSpectrum_cn.h"
#include "TISP_logMelSpectrum_idat.h"

#include "../../../src/FFTLIB/TISP_twiddle_lut.hpp"
#include "../../../src/common/TISP_mel_fbank_gen.hpp"
#include "../../../src/common/TISP_window_gen.hpp"
#include "common/AUDIOLIB_bufParams.h"

#include "../common/TI_memory.h"

#if !defined(_HOST_BUILD)
#include <kernel/dpl/HwiP.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

/* Node available: exercise opt-vs-cn and opt-vs-static (mirrors TISP_stft_test.cpp). */
#define TISP_LOGMELSPECTRUM_NODE_AVAILABLE 1

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) /* 1 MB */
#elif SOC == AM62A || SOC == AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) /* 800 KB */
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (1024 * 1024) /* 1 MB */
#endif

#ifdef WIN32
#if defined(__C7504__)
int8_t l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t ddrBuffer[2048 * 1024];
#elif defined(__C7524__)
int8_t l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t ddrBuffer[1 * 1024 * 1024];
#else
int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
int8_t ddrBuffer[2048 * 1024];
#endif
#else
#if defined(__C7504__)
__attribute__((section(".l2sramaux"), aligned(64))) int8_t  l2auxBuffer[TISP_L2_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t    ddrBuffer[2048 * 1024];
__attribute__((section(".l2sramData"), aligned(64))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];
#elif defined(__C7524__)
__attribute__((section(".ddrData"), aligned(128))) int8_t    ddrBuffer[1 * 1024 * 1024];
__attribute__((section(".l2sramData"), aligned(128))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];
#else
__attribute__((section(".msmcData"), aligned(64))) int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t  ddrBuffer[2048 * 1024];
#endif
#endif /* WIN32 */

int16_t volatile volatileSum = 0;

/* geometry + scratch come from the node's static planners
 * (TISP::AUDIOLIB::LogMelSpectrum<float>::calc*), so the harness and the node
 * agree and the scratch is sized for the node's full pipeline (the cn only uses
 * a prefix of the same buffer). */
using LMS = TISP::AUDIOLIB::LogMelSpectrum<float>;

int TISP_logMelSpectrum_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   (void) LevelOfFeedback;

   int32_t tpi;
   int32_t currentTestFail;
   int32_t fail = 0;

   TISP_logMelSpectrum_testParams_t *prm;
   TISP_logMelSpectrum_testParams_t  currPrm;
   TISP_logMelSpectrum_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_logMelSpectrum");

   FILE *fpOutputCSV = fopen("TISP_logMelSpectrum.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, fftSize, hopSize, numCh, numInputSamples, numMelBins, "
                        "Windows per clip, EVM cold cycles, cycles/frame, cycles/sample, Pass/Fail\n");

   UnitTestL2Heap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      currPrm = prm[tpi];

      uint32_t         fftSize            = currPrm.fftSize;
      uint32_t         windowLength       = currPrm.windowLength;
      uint32_t         hopSize            = currPrm.hopSize;
      uint32_t         numChannels        = currPrm.numChannels;
      uint32_t         numInputSamples    = currPrm.numInputSamples;
      uint32_t         numMelBins         = currPrm.numMelBins;
      uint32_t         sampleRate         = currPrm.sampleRate;
      float            fMin               = currPrm.fMin;
      float            fMax               = currPrm.fMax;
      float            power              = currPrm.power;
      float            logOffset          = currPrm.logOffset;
      uint32_t         melScale           = currPrm.melScale;
      uint32_t         melNorm            = currPrm.melNorm;
      uint32_t         numBlocks          = currPrm.numBlocks;
      TISP::WindowType winType            = static_cast<TISP::WindowType>(currPrm.windowType);
      uint32_t         windowAlign        = currPrm.windowAlign;
      bool             windowPeriodic     = (currPrm.windowPeriodic != 0u);

      uint32_t numSpectrogramBins = LMS::calcNumSpectrogramBins(fftSize);

      /* derived geometry (node static planners) */
      uint32_t numWindows     = LMS::calcNumWindows(fftSize, hopSize, numInputSamples);
      uint32_t overlapBufSize = LMS::calcOverlapBufSize(fftSize, hopSize, numInputSamples);

      /* buffer sizes in floats (one block held at a time) */
      size_t inputSize   = (size_t) numChannels * numInputSamples;
      size_t outputSize  = (size_t) numChannels * numWindows * numMelBins;
      size_t windowSize  = fftSize;
      size_t melSize     = (size_t) numSpectrogramBins * numMelBins;
      size_t stateSize   = (size_t) overlapBufSize * numChannels;
      size_t scratchSize = LMS::calcScratchSize(fftSize, hopSize, numInputSamples, numMelBins, numChannels);

      /* allocate from l2 heap (element count — allocator multiplies by sizeof(T)) */
      auto pX       = myL2Heap.allocate64ByteAligned<float>(inputSize);
      auto pY       = myL2Heap.allocate64ByteAligned<float>(outputSize);
      auto pWindow  = myL2Heap.allocate64ByteAligned<float>(windowSize);
      auto pMel     = myL2Heap.allocate64ByteAligned<float>(melSize);
      auto pState   = myL2Heap.allocate64ByteAligned<float>(stateSize == 0 ? 1 : stateSize);
      auto pScratch = myL2Heap.allocate64ByteAligned<float>(scratchSize);

      /* cn reference buffers in ddr (pYCn + pStateCn) */
      float *pYCn     = (float *) ddrBuffer;
      float *pStateCn = pYCn + outputSize;

      currentTestFail = 0;

      if (pX.get() && pY.get() && pWindow.get() && pMel.get() && pState.get() && pScratch.get()) {

         /* window at runtime (power = 1.0) — center-padded or left-aligned per params */
         TISP::WindowAlign winAlign =
             (windowAlign == 1u) ? TISP::WindowAlign::kLeft : TISP::WindowAlign::kCenter;
         TISP::windowGen<float>(pWindow.get(), fftSize, windowLength, winType, windowPeriodic, winAlign);

         /* mel filterbank at runtime (matches torchaudio.melscale_fbanks) */
         TISP::melFbankGen<float>(pMel.get(), numSpectrogramBins, numMelBins, sampleRate, fMin, fMax,
                                  static_cast<TISP::MelScale>(melScale), static_cast<TISP::MelNorm>(melNorm));

         /* ---- cn reference: run per block (streaming state carried across blocks) ---- */
         int32_t status_ref_vs_cn = TI_TEST_KERNEL_PASS;

         memset(pStateCn, 0, stateSize * sizeof(float));

         for (uint32_t blockIdx = 0; blockIdx < numBlocks; blockIdx++) {
            float *pBlockIn = &currPrm.staticIn[blockIdx * inputSize];

            memset(pYCn, 0, outputSize * sizeof(float));

            uint32_t coldStart = (blockIdx == 0u) ? 1u : 0u;

            if (coldStart != 0u) {
               TI_profile_start(TI_PROFILE_KERNEL_OPT);
            }

            AUDIOLIB_logMelSpectrum_cn(pBlockIn, pYCn, pWindow.get(), pMel.get(), pStateCn, (float *) pScratch.get(),
                                       fftSize, hopSize, numChannels, numInputSamples, numMelBins, power, logOffset);

            if (coldStart != 0u) {
               TI_profile_stop();
            }

            /* compare cn vs static python reference for this block */
            if (currPrm.staticOut != NULL) {
               float  *pExpected  = &currPrm.staticOut[blockIdx * outputSize];
               int32_t blockStatus = TI_compare_mem_2D_float((void *) pYCn, (void *) pExpected, 0.001,
                                                             (double) powf(2, -10), outputSize, 1,
                                                             outputSize * sizeof(float), sizeof(float));

               if (blockStatus == TI_TEST_KERNEL_FAIL) {
                  status_ref_vs_cn = TI_TEST_KERNEL_FAIL;
               }
            }
         }

         int32_t status_opt_vs_cn  = TI_TEST_KERNEL_PASS;
         int32_t status_opt_vs_ref = TI_TEST_KERNEL_PASS;

#if defined(TISP_LOGMELSPECTRUM_NODE_AVAILABLE)
         /* ---- optimized node path (mirrors TISP_stft_test.cpp) ---- */
         memset(pY.get(), 0, outputSize * sizeof(float));

         TI_profile_start(TI_PROFILE_KERNEL_INIT);
         TISP::ErrorCtxt errorCtx;
         auto            logMelNode = TISP::make_unique_node<TISP::AUDIOLIB::LogMelSpectrum<float>>(
             (float *) pX.get(), (float *) pY.get(), (float *) pWindow.get(), (float *) pMel.get(),
             (float *) pState.get(), (float *) pScratch.get(), fftSize, hopSize, numChannels, numInputSamples,
             numMelBins, power, logOffset, "TISP_logMelSpectrum", currPrm.testID, errorCtx);
         TI_profile_stop();

         if ((logMelNode == nullptr) || (!errorCtx.isSuccess())) {
            printf("\nError creating logMelNode: %s\n", errorCtx.getMessage());
            return -1;
         }

         memset(pState.get(), 0, stateSize * sizeof(float));
         memset(pStateCn, 0, stateSize * sizeof(float));

         for (uint32_t blockIdx = 0; blockIdx < numBlocks; blockIdx++) {
            float *pBlockIn = &currPrm.staticIn[blockIdx * inputSize];

            memcpy(pX.get(), pBlockIn, inputSize * sizeof(float));
            memset(pY.get(), 0, outputSize * sizeof(float));
            logMelNode->exec();

            memset(pYCn, 0, outputSize * sizeof(float));
            AUDIOLIB_logMelSpectrum_cn(pBlockIn, pYCn, pWindow.get(), pMel.get(), pStateCn, (float *) pScratch.get(),
                                       fftSize, hopSize, numChannels, numInputSamples, numMelBins, power, logOffset);

            if (TI_compare_mem_2D_float((void *) pY.get(), (void *) pYCn, 0.001, (double) powf(2, -10), outputSize, 1,
                                        outputSize * sizeof(float), sizeof(float)) == TI_TEST_KERNEL_FAIL) {
               status_opt_vs_cn = TI_TEST_KERNEL_FAIL;
            }

            if (currPrm.staticOut != NULL) {
               float *pExpected = &currPrm.staticOut[blockIdx * outputSize];
               if (TI_compare_mem_2D_float((void *) pY.get(), (void *) pExpected, 0.001, (double) powf(2, -10),
                                           outputSize, 1, outputSize * sizeof(float), sizeof(float)) ==
                   TI_TEST_KERNEL_FAIL) {
                  status_opt_vs_ref = TI_TEST_KERNEL_FAIL;
               }
            }
         }
#else
         (void) pX;
         (void) pState;
#endif /* TISP_LOGMELSPECTRUM_NODE_AVAILABLE */

         currentTestFail = ((status_ref_vs_cn == TI_TEST_KERNEL_FAIL) || (status_opt_vs_cn == TI_TEST_KERNEL_FAIL) ||
                            (status_opt_vs_ref == TI_TEST_KERNEL_FAIL))
                               ? 1
                               : 0;

         fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

         int32_t coldCycles    = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[3 * tpi]     = coldCycles;
         pProfile[3 * tpi + 1] = coldCycles;
         pProfile[3 * tpi + 2] = coldCycles;

         float cyclesPerFrame =
             (numWindows > 0) ? (float) coldCycles / (float) (numWindows * numChannels) : 0.0f;
         float cyclesPerSample =
             (numInputSamples > 0) ? (float) coldCycles / (float) (numInputSamples * numChannels) : 0.0f;

         sprintf(desc, "fftSize=%u hopSize=%u numCh=%u numInputSamples=%u numMelBins=%u numWindows=%u numBlocks=%u",
                 fftSize, hopSize, numChannels, numInputSamples, numMelBins, numWindows * numChannels, numBlocks);

         fprintf(fpOutputCSV, "TISP_logMelSpectrum, %u, %u, %u, %u, %u, %u, %u, %d, %.2f, %.2f, %d\n", currPrm.testID,
                 fftSize, hopSize, numChannels, numInputSamples, numMelBins, numWindows * numChannels, coldCycles,
                 cyclesPerFrame, cyclesPerSample, !currentTestFail);

         TI_profile_add_test(currPrm.testID, numChannels * numWindows * numMelBins, 0, 0, currentTestFail, desc);
      }
      else {
         sprintf(desc, "fftSize=%u hopSize=%u numCh=%u numInputSamples=%u numMelBins=%u numBlocks=%u", fftSize, hopSize,
                 numChannels, numInputSamples, numMelBins, numBlocks);
         TI_profile_skip_test(desc);
         TI_profile_clear_run_stats();
      }

      /* Reclaim the twiddle arena between cases so it does not accumulate an
       * entry per unique FFT size across all test cases (the node builds an
       * internal RFFT1dBatched each construction). */
      TISP::FFTLIB::TwiddleLUT<float>::reset();
   }

   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("Running on target (C7x DSP).\n");
#endif
   return TISP_logMelSpectrum_d(&pProfile[0], 0);
}

#ifdef __cplusplus
extern "C" {
#endif

int test_main_d()
{
   int      fail = 1;
   uint32_t profile[256 * 3];

   fail = test_main(&profile[0]);

#if !defined(NO_PRINTF)
   if (fail == 0) {
      printf("Test Pass!\n");
   }
   else {
      printf("Test Fail!\n");
   }

   int testCaseIdx;

   for (testCaseIdx = 0; testCaseIdx < test_cases; testCaseIdx++) {
      printf("Test %4d: Cold Cycles = %8d, Warm Cycles = %8d, Warm Cycles WRB = %8d\n", testCaseIdx,
             profile[3 * testCaseIdx], profile[3 * testCaseIdx + 1], profile[3 * testCaseIdx + 2]);
   }
#endif

   return 1;
}

#ifdef __cplusplus
}
#endif

/* ======================================================================== */
/*  End of file:  TISP_logMelSpectrum_test.cpp                              */
/* ======================================================================== */
