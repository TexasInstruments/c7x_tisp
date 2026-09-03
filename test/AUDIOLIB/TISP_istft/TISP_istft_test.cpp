// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <audiolib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <tisp.hpp>

#include "TISP_istft_cn.h"
#include "TISP_istft_idat.h"

#include "common/TISP_window_gen.hpp"
#include "common/AUDIOLIB_bufParams.h"

#include "../common/TI_memory.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#if !defined(_HOST_BUILD)
#include <kernel/dpl/HwiP.h>
#endif

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) /* 1 MB */
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) /* 800 KB */
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) /* 2 MB */
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

int TISP_istft_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t tpi;
   int32_t currentTestFail;
   int32_t fail = 0;
   float  *pExpected, snr_ref_vs_opt = 0.0, snr_ref_vs_natc = 0.0, snr_natc_vs_opt = 0.0;

   TISP_istft_testParams_t *prm;
   TISP_istft_testParams_t  currPrm;
   TISP_istft_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_istft");

   FILE *fpOutputCSV = fopen("TISP_istft.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, fftSize, hopSize, numCh, numWindows, "
                        "onesided, numBlocks, Total L2 Memory Usage (kB), "
                        "EVM cycles, cycles/fft_window, cycles/output_sample, "
                        "snr_ref_vs_opt, snr_ref_vs_natc, snr_natc_vs_opt, Pass/Fail\n");

   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      currPrm = prm[tpi];

      uint32_t         fftSize        = currPrm.fftSize;
      uint32_t         hopSize        = currPrm.hopSize;
      uint32_t         numChannels    = currPrm.numChannels;
      uint32_t         numWindows     = currPrm.numWindows;
      bool             onesided       = (currPrm.onesided != 0u);
      uint32_t         numBlocks      = currPrm.numBlocks;
      uint32_t         inBins         = onesided ? (fftSize + 2u) : (2u * fftSize);
      TISP::WindowType winType        = static_cast<TISP::WindowType>(currPrm.windowType);
      uint32_t         windowLength   = currPrm.windowLength;
      uint32_t         windowAlign    = currPrm.windowAlign;
      bool             windowPeriodic = (currPrm.windowPeriodic != 0u);

      /* derived constants via Istft static utilities */
      uint32_t overlapBufSize   = TISP::AUDIOLIB::Istft<float>::calcOlaBufSize(fftSize, hopSize);
      uint32_t numOutputSamples = TISP::AUDIOLIB::Istft<float>::calcNumOutputSamples(numWindows, hopSize);

      /* buffer sizes in floats (pX/pY hold one block at a time) */
      size_t inputSize   = (size_t) numChannels * numWindows * inBins;
      size_t outputSize  = (size_t) numChannels * numOutputSamples;
      size_t windowSize  = fftSize;
      size_t olaBufSize  = (size_t) overlapBufSize * numChannels;
      size_t scratchSize = TISP::AUDIOLIB::Istft<float>::calcScratchSize(fftSize, numWindows, numChannels);

      float totalMemKB =
          TISP::AUDIOLIB::Istft<float>::calcTotalMemKB(fftSize, hopSize, numChannels, numWindows, onesided);

      /* allocate from l2 heap (element count, not bytes — allocator multiplies by sizeof(T)) */
      auto pX       = myL2Heap.allocate64ByteAligned<float>(inputSize);
      auto pY       = myL2Heap.allocate64ByteAligned<float>(outputSize);
      auto pWindow  = myL2Heap.allocate64ByteAligned<float>(windowSize);
      auto pOlaBuf  = myL2Heap.allocate64ByteAligned<float>(olaBufSize > 0u ? olaBufSize : 1u);
      auto pScratch = myL2Heap.allocate64ByteAligned<float>(scratchSize);

      /* cn reference buffers — use ddr (pYCn + pOlaBufCn) */
      float *pYCn      = (float *) ddrBuffer;
      float *pOlaBufCn = pYCn + outputSize;

      currentTestFail = 0;

      if (pX.get() && pY.get() && pWindow.get() && pOlaBuf.get() && pScratch.get()) {

         /* generate synthesis window at runtime (power=1.0).
          * windowAlign: 0 = center-padded (PyTorch/librosa); 1 = left-aligned (scipy/conv-STFT)
          * windowPeriodic: 1 = DFT-even (torch periodic / scipy fftbins=True);
          *                 0 = symmetric (torch periodic=False / scipy sym=True, e.g. gcrn) */
         TISP::WindowAlign winAlign = (windowAlign == 1u) ? TISP::WindowAlign::kLeft : TISP::WindowAlign::kCenter;
         TISP::windowGen<float>(pWindow.get(), fftSize, windowLength, winType, windowPeriodic, winAlign);

         /* fill first block's input */
         memcpy(pX.get(), &currPrm.staticIn[0], inputSize * sizeof(float));

         memset(pY.get(), 0, outputSize * sizeof(float));

         /* create istft node (constructor zero-initializes pOlaBuf) */
         TI_profile_start(TI_PROFILE_KERNEL_INIT);

         TISP::ErrorCtxt errorCtx;
         auto            istftNode = std::make_unique<TISP::AUDIOLIB::Istft<float>>(
             (float *) pX.get(), (float *) pY.get(), (float *) pWindow.get(), (float *) pOlaBuf.get(),
             (float *) pScratch.get(), fftSize, hopSize, numChannels, numWindows, onesided, "TISP_istft",
             currPrm.testID, errorCtx);

         TI_profile_stop();

         if ((istftNode == nullptr) || (!errorCtx.isSuccess())) {
            printf("\nError creating istftNode: %s\n", errorCtx.getMessage());
            return -1;
         }

         /* first exec — cold cache (block 0 only) */
         TI_profile_start(TI_PROFILE_KERNEL_OPT);
         istftNode->exec();
         TI_profile_stop();

         /* warm cache runs — take minimum across NUM_REPS */
         uint64_t minWarmCycles = UINT64_MAX;

         for (int32_t repCount = 0; repCount < NUM_REPS; repCount++) {
            memset(pOlaBuf.get(), 0, olaBufSize * sizeof(float));

            for (uint32_t blockIdx = 0; blockIdx < numBlocks; blockIdx++) {
               memcpy(pX.get(), &currPrm.staticIn[blockIdx * inputSize], inputSize * sizeof(float));
               memset(pY.get(), 0, outputSize * sizeof(float));

               TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);
#if !defined(_HOST_BUILD)
               uintptr_t hwiKey = HwiP_disable();
#endif
               istftNode->exec();
#if !defined(_HOST_BUILD)
               HwiP_restore(hwiKey);
#endif
               TI_profile_stop();
            }

            if (cycles[TI_PROFILE_KERNEL_OPT_WARM] < minWarmCycles) {
               minWarmCycles = cycles[TI_PROFILE_KERNEL_OPT_WARM];
            }
         }

         cycles[TI_PROFILE_KERNEL_OPT_WARM] = minWarmCycles;

         /* get output to l1d */
         int16_t outSum   = 0;
         int8_t *pOutTemp = (int8_t *) pY.get();

         for (size_t k = 0; k < outputSize * sizeof(float); k++) {
            outSum += *pOutTemp;
            pOutTemp++;
         }

         volatileSum = outSum;

         /* warm with write-back — single exec, cache already warm */
         memset(pOlaBuf.get(), 0, olaBufSize * sizeof(float));
         memcpy(pX.get(), &currPrm.staticIn[0], inputSize * sizeof(float));
         memset(pY.get(), 0, outputSize * sizeof(float));

         TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARMWRB);
         TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);
         istftNode->exec();
         TI_profile_stop();

         /*
          * functional comparison — run opt and cn in lockstep per block,
          * comparing each block's output against cn and static reference
          */
         int32_t status_nat_vs_opt = TI_TEST_KERNEL_PASS;
         int32_t status_ref_vs_opt = TI_TEST_KERNEL_PASS;

         /* reset state for both opt and cn */
         memset(pOlaBuf.get(), 0, olaBufSize * sizeof(float));
         memset(pOlaBufCn, 0, olaBufSize * sizeof(float));

         for (uint32_t blockIdx = 0; blockIdx < numBlocks; blockIdx++) {
            float *pBlockIn = &currPrm.staticIn[blockIdx * inputSize];

            /* opt exec */
            memcpy(pX.get(), pBlockIn, inputSize * sizeof(float));
            memset(pY.get(), 0, outputSize * sizeof(float));
            istftNode->exec();

            /* cn exec (uses separate pOlaBufCn) */
            memset(pYCn, 0, outputSize * sizeof(float));
            AUDIOLIB_istft_cn(pBlockIn, pYCn, pWindow.get(), pOlaBufCn, fftSize, hopSize, numChannels, numWindows,
                              onesided);

            /* compare opt vs cn for this block */
            int32_t blockStatus =
                TI_compare_mem_2D_float((void *) pY.get(), (void *) pYCn, 0.001, (double) powf(2, -10), outputSize, 1,
                                        outputSize * sizeof(float), sizeof(float));

            if (blockStatus == TI_TEST_KERNEL_FAIL) {
               status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
            }

            /* compare opt vs static reference for this block */
            if (currPrm.staticOut != NULL) {
               pExpected = &currPrm.staticOut[blockIdx * outputSize];
               blockStatus =
                   TI_compare_mem_2D_float((void *) pY.get(), (void *) pExpected, 0.001, (double) powf(2, -10),
                                           outputSize, 1, outputSize * sizeof(float), sizeof(float));

               if (blockStatus == TI_TEST_KERNEL_FAIL) {
                  status_ref_vs_opt = TI_TEST_KERNEL_FAIL;
               }
            }
         }

         snr_ref_vs_opt  = TI_Profile_get_snr(pExpected, pY.get(), outputSize);
         snr_ref_vs_natc = TI_Profile_get_snr(pExpected, pYCn, outputSize);
         snr_natc_vs_opt = TI_Profile_get_snr(pYCn, pY.get(), outputSize);

         currentTestFail =
             ((status_nat_vs_opt == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt == TI_TEST_KERNEL_FAIL)) ? 1 : 0;

         fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

         pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
         pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

         sprintf(desc,
                 "snr_ref_vs_opt=%.2f snr_ref_vs_natc=%.2f snr_natc_vs_opt=%.2f fftSize=%u hopSize=%u numCh=%u "
                 "numWindows=%u onesided=%u numBlocks=%u",
                 snr_ref_vs_opt, snr_ref_vs_natc, snr_natc_vs_opt, fftSize, hopSize, numChannels, numWindows,
                 currPrm.onesided, numBlocks);

         int32_t warmCycles         = pProfile[3 * tpi + 1];
         float   cyclesPerFFTWindow = (numWindows > 0) ? (float) warmCycles / (float) (numWindows * numChannels) : 0.0f;
         float   cyclesPerSample =
             (numOutputSamples > 0) ? (float) warmCycles / (float) (numOutputSamples * numChannels) : 0.0f;

         fprintf(fpOutputCSV,
                 "TISP_istft, %u, %u, %u, %u, %u, "
                 "%u, %u, %.2f, %d, %.2f, %.2f, %.2f, %.2f, %.2f, %d\n",
                 currPrm.testID, fftSize, hopSize, numChannels, numWindows, currPrm.onesided, numBlocks, totalMemKB,
                 warmCycles, cyclesPerFFTWindow, cyclesPerSample, snr_ref_vs_opt, snr_ref_vs_natc, snr_natc_vs_opt,
                 !currentTestFail);

         TI_profile_add_test(currPrm.testID, numChannels * numWindows * fftSize, 0, 0, currentTestFail, desc);

         TISP::FFTLIB::TwiddleLUT<float>::clear();
      }
      else {
         sprintf(desc, "fftSize=%u hopSize=%u numCh=%u numWindows=%u numBlocks=%u", fftSize, hopSize, numChannels,
                 numWindows, numBlocks);
         TI_profile_skip_test(desc);
         TI_profile_clear_run_stats();
      }
   }

   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("Running on target (C7x DSP).\n");
#endif
   return TISP_istft_d(&pProfile[0], 0);
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

   int i;

   for (i = 0; i < test_cases; i++) {
      printf("Test %4d: Cold Cycles = %8d, Warm Cycles = %8d, Warm Cycles WRB = %8d\n", i, profile[3 * i],
             profile[3 * i + 1], profile[3 * i + 2]);
   }
#endif

   return fail;
}

#ifdef __cplusplus
}
#endif
