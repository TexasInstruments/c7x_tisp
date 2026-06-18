// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_istft_superNode_idat.h"
#include "TISP_test.h"
#include <audiolib.h>
#include <tisp.hpp>
#include <TISP_ErrorCtxt.hpp>

#include "../../../src/common/TISP_window_gen.hpp"
#include "../../../src/superNode/TISP_dmaUtils.hpp"
#include "TISP_superNode.hpp"
#include <TISP_istft.hpp>
#include <kernel/dpl/CacheP.h>

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) // 2 MB
#endif

#define TOTAL_DMA_CHANNELS (2)

#ifdef WIN32
#if defined(__C7504__)
int8_t l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t ddrBuffer[32 * 1024 * 1024];
#elif defined(__C7524__)
int8_t l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t ddrBuffer[1 * 1024 * 1024];
#else
int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
int8_t ddrBuffer[32 * 1024 * 1024];
#endif
#else
#if defined(__C7504__)
__attribute__((section(".l2sramaux"), aligned(64))) int8_t  l2auxBuffer[TISP_L2_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t    ddrBuffer[32 * 1024 * 1024];
__attribute__((section(".l2sramData"), aligned(64))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];
#elif defined(__C7524__)
__attribute__((section(".ddrData"), aligned(128))) int8_t    ddrBuffer[1 * 1024 * 1024];
__attribute__((section(".l2sramData"), aligned(128))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];
#else
__attribute__((section(".msmcData"), aligned(64))) int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t  ddrBuffer[32 * 1024 * 1024];
#endif
#endif /* WIN32 */


int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

static TISP::SuperNode::superNodeVec *TISP_istft_createSuperNode(void *pIn,
                                                                 void *pOut,
                                                                 void *pInBlock,
                                                                 void *pOutBlock,
                                                                 void *pWindow,
                                                                 void *pOlaBuf,
                                                                 void *pScratch,
                                                                 uint32_t fftSize,
                                                                 uint32_t hopSize,
                                                                 uint32_t numChannels,
                                                                 uint32_t numWindows,
                                                                 bool     onesided,
                                                                 uint32_t inBins,
                                                                 uint32_t numBlocks,
                                                                 size_t   inPitch,
                                                                 size_t   outPitch,
                                                                 TISP::ErrorCtxt &errorCtx)
{
   auto k0 = std::make_unique<TISP::AUDIOLIB::Istft<float>>(
       (float *) pInBlock, (float *) pOutBlock, (float *) pWindow, (float *) pOlaBuf, (float *) pScratch, fftSize,
       hopSize, numChannels, numWindows, onesided, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating Istft<float> node: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto myOpVec = std::make_unique<TISP::opVec>();
   myOpVec->push_back(std::move(k0));

   size_t widthIn        = (size_t) numWindows * inBins * numBlocks;
   size_t heightIn       = numChannels;
   size_t widthOut       = (size_t) numWindows * hopSize * numBlocks;
   size_t heightOut      = numChannels;
   size_t blockWidthIn   = (size_t) numWindows * inBins;
   size_t blockHeightIn  = numChannels;
   size_t blockWidthOut  = (size_t) numWindows * hopSize;
   size_t blockHeightOut = numChannels;

   auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dDiffDims<float, float>>(
       std::move(myOpVec), (float *) pIn, (float *) pOut, (float *) pInBlock, (float *) pOutBlock, widthIn, heightIn,
       widthOut, heightOut, blockWidthIn, blockHeightIn, blockWidthOut, blockHeightOut, inPitch, outPitch,
       TOTAL_DMA_CHANNELS);

   auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();
   mySuperNodeVec->push_back(std::move(s0));

   return mySuperNodeVec;
}

int TISP_istft_superNode_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t k    = 0;
   uint32_t testNum;
   uint64_t archCycles     = 0;
   float    snr_ref_vs_opt = 0.0;

   TISP_istft_superNode_testParams_t *prm;
   TISP_istft_superNode_testParams_t  currPrm;
   TISP_istft_superNode_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_istft_superNode");

   FILE *fpOutputCSV = fopen("TISP_istft_superNode.csv", "w+");
   fprintf(fpOutputCSV,
           "Kernel, Test ID, Data size, Arch cycles, Warm cycles, Warm/Arch cycles, snr_ref_vs_opt, Pass/Fail\n");

   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

#ifdef HOST_EMULATION
   /* host: DMA utils are stubbed */
#elif __C7X_VEC_SIZE_BITS__ == 512
   int32_t                retVal = 0;
   Sciclient_ConfigPrms_t sciClientCfg;
   Sciclient_configPrmsInit(&sciClientCfg);
   retVal = Sciclient_init(&sciClientCfg);
   if (retVal != 0) {
      printf("Sciclient Init Failed \n");
   }
   TISP::test_sciclientDmscGetVersion(NULL, 0);
   TISP::appC7xClecInitDru();
#else
   App_dmautilsClecInitDru();
#endif

   for (tpi = 0; tpi < test_cases; tpi++) {
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;
      currentTestFail           = 0;

      uint32_t fftSize      = currPrm.fftSize;
      uint32_t windowLength = currPrm.windowLength;
      uint32_t hopSize      = currPrm.hopSize;
      uint32_t numChannels  = currPrm.numChannels;
      uint32_t numWindows   = currPrm.numWindows;
      bool     onesided     = (currPrm.onesided != 0u);
      uint32_t numBlocks    = currPrm.numBlocks;

      size_t datasize = sizeof(float);

      /* derived geometry (single source of truth = the ISTFT node's own static helpers) */
      uint32_t inBins         = onesided ? (fftSize + 2u) : (2u * fftSize);
      uint32_t overlapBufSize = TISP::AUDIOLIB::Istft<float>::calcOlaBufSize(fftSize, hopSize);
      uint32_t scratchSize    = TISP::AUDIOLIB::Istft<float>::calcScratchSize(fftSize, numWindows, numChannels);

      size_t inPitch  = currPrm.dataPitchIn;  /* = numWindows*inBins*numBlocks*sizeof(float) */
      size_t outPitch = currPrm.dataPitchOut; /* = numWindows*hopSize*numBlocks*sizeof(float) */

      size_t outputSize = (size_t) numWindows * hopSize * numChannels * numBlocks * datasize;
      size_t outputsize = (size_t) numWindows * hopSize * numChannels * numBlocks;

      /* ping-pong L2 block buffers (2x) + fixed L2 window/ola/scratch buffers */
      auto pInBlock  = myL2Heap.allocate64ByteAligned<uint8_t>(2u * numWindows * inBins * numChannels * datasize);
      auto pOutBlock = myL2Heap.allocate64ByteAligned<uint8_t>(2u * numWindows * hopSize * numChannels * datasize);
      auto pWindow   = myL2Heap.allocate64ByteAligned<uint8_t>(fftSize * datasize);
      auto pOlaBuf   = myL2Heap.allocate64ByteAligned<uint8_t>((size_t) overlapBufSize * numChannels * datasize);
      auto pScratch  = myL2Heap.allocate64ByteAligned<uint8_t>((size_t) scratchSize * datasize);

      void *pOut = (void *) ddrBuffer;
      memset(pOut, 0, outputSize);

      /* Only run the test if the buffer allocations fit in the heap */
      if (pInBlock.get() && pOutBlock.get() && pWindow.get() && pOlaBuf.get() && pScratch.get() && pOut) {

         TI_profile_start(TI_PROFILE_KERNEL_INIT);

         /* build the synthesis window on the host side, exactly as the ISTFT node test does */
         TISP::WindowType  winType  = static_cast<TISP::WindowType>(currPrm.windowType);
         TISP::WindowAlign winAlign = (currPrm.windowAlign == 1u) ? TISP::WindowAlign::kLeft : TISP::WindowAlign::kCenter;
         bool              periodic = (currPrm.windowPeriodic != 0u);
         TISP::windowGen<float>((float *) pWindow.get(), fftSize, windowLength, winType, periodic, winAlign);

         /* zero OLA state so the first block starts from silence (matches the reference) */
         memset(pOlaBuf.get(), 0, (size_t) overlapBufSize * numChannels * datasize);

         TISP::ErrorCtxt errorCtx;
         TISP::SuperNode::superNodeVec *mySuperNodeVec = TISP_istft_createSuperNode(
             (void *) currPrm.staticIn, pOut, pInBlock.get(), pOutBlock.get(), pWindow.get(), pOlaBuf.get(),
             pScratch.get(), fftSize, hopSize, numChannels, numWindows, onesided, inBins, numBlocks, inPitch, outPitch,
             errorCtx);

         if (!errorCtx.isSuccess()) {
            printf("\nError creating supernode: %s\n", errorCtx.getMessage());
            currentTestFail = 1;
            fail            = 1;
            continue;
         }

         TI_profile_stop();

         TI_profile_start(TI_PROFILE_KERNEL_OPT);
         TISP::execute::graph(mySuperNodeVec);
         TI_profile_stop();

         /* re-zero OLA state between runs so each full pass is independent */
         memset(pOlaBuf.get(), 0, (size_t) overlapBufSize * numChannels * datasize);

         /* warm runs to train the branch predictor */
         for (k = 0; k < NUM_REPS; k++) {
            TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
            memset(pOlaBuf.get(), 0, (size_t) overlapBufSize * numChannels * datasize);
            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);
            TISP::execute::graph(mySuperNodeVec);
            TI_profile_stop();
         }

         /* get output to L1D */
         int16_t outSum   = 0;
         int8_t *pOutTemp = (int8_t *) pOut;
         for (k = 0; k < outputSize; k++) {
            outSum += *pOutTemp;
            pOutTemp++;
         }
         volatileSum = outSum;

         /* warm instruction-cache run (this is the pass compared against the reference) */
         memset(pOlaBuf.get(), 0, (size_t) overlapBufSize * numChannels * datasize);
         TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);
         TISP::execute::graph(mySuperNodeVec);
         TI_profile_stop();

         if (currPrm.staticOut != nullptr) {
            status_ref_vs_opt = TI_compare_mem_roi2DStatic_float(
                (void *) pOut, (void *) currPrm.staticOut, 0.001, (double) powf(2, -10),
                numWindows * hopSize * numBlocks, numChannels, outPitch, datasize);
         }
         else {
            status_ref_vs_opt = TI_TEST_KERNEL_PASS;
         }

         currentTestFail = ((status_ref_vs_opt == TI_TEST_KERNEL_FAIL) || (currentTestFail == 1)) ? 1 : 0;
         fail            = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

         snr_ref_vs_opt = TI_Profile_get_snr(currPrm.staticOut, pOut, outputsize);

         pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
         pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

         sprintf(desc, "%s generated input | snr_ref_vs_opt=%.2f fftSize=%u hop=%u numCh=%u W=%u numBlocks=%u",
                 testPatternString, snr_ref_vs_opt, fftSize, hopSize, numChannels, numWindows, numBlocks);

         fprintf(fpOutputCSV, "TISP_istft_superNode, %d, %lu, %lu, %lu, %.2f, %.2f, %d\n", testNum,
                 (unsigned long) (numWindows * hopSize * numChannels * numBlocks), (unsigned long) archCycles,
                 (unsigned long) cycles[TI_PROFILE_KERNEL_OPT_WARM],
                 ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles), snr_ref_vs_opt, !currentTestFail);

         TI_profile_add_test(testNum++, numWindows * hopSize * numChannels * numBlocks, 0, 0, currentTestFail, desc);

         TISP::FFTLIB::TwiddleLUT<float>::clear();

         delete mySuperNodeVec;
      }
      else {
         sprintf(desc, "fftSize=%u, numCh=%u, W=%u (alloc too large)", fftSize, numChannels, numWindows);
         TI_profile_skip_test(desc);
         TI_profile_clear_run_stats();
      } // end of memory allocation successful?
   }    // end idat test cases

   fclose(fpOutputCSV);
   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("Running on target (C7x DSP).\n");
#endif
   return TISP_istft_superNode_d(&pProfile[0], 0);
}

/* Main call for individual test projects */
#ifdef __cplusplus
extern "C" {
#endif
int test_main_d()
{
   int fail = 1;

   uint32_t profile[256 * 3];

   fail = test_main(&profile[0]);

#if !defined(NO_PRINTF)
   if (fail == 0)
      printf("Test Pass!\n");
   else
      printf("Test Fail!\n");

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
