// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include AUDIOLIB
#include <audiolib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <cstring>
#include <tisp.hpp>

// include test data for this kernel
#include "TISP_limiter_cn.h"
#include "TISP_limiter_idat.h"
#include "common/AUDIOLIB_bufParams.h"

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) // 2MB KB
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
#endif // WIN32

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

int TISP_limiter_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t testNum;
   uint64_t estCycles  = 0;

   TISP_limiter_testParams_t *prm;
   TISP_limiter_testParams_t  currPrm;
   TISP_limiter_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_limiter");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_limiter.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "Num channels, Block size, Interleaved data mode, Warm cycles, Perf est cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testId;
      currPrm = prm[tpi];

      size_t numChannels = currPrm.numChannels;
      size_t blockSize   = currPrm.blockSize;

      for (repCount = 0; repCount < numReps; repCount++) {
         int32_t status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_nat = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;
         currentTestFail        = 0;

         size_t dataSize = AUDIOLIB_sizeof(currPrm.dataType);

         size_t inputSize, gainSize, outputSize, scratchBufSize;
         size_t paramSize;

         inputSize      = blockSize * numChannels * dataSize;
         gainSize       = numChannels * dataSize;
         outputSize     = blockSize * numChannels * dataSize;
         scratchBufSize = blockSize * numChannels * dataSize;
         paramSize      = numChannels * dataSize;

         // TODO: add pitch to test case csv
         size_t   pitchX        = currPrm.pitchX;
         size_t   pitchS        = currPrm.pitchS;
         size_t   pitchY        = currPrm.pitchY;
         bool     isInterleaved = currPrm.isInterleaved;
         uint32_t bypassMode    = currPrm.bypassMode;

         auto pIn           = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pScratch      = myL2Heap.allocate64ByteAligned<uint8_t>(scratchBufSize);
         auto pGainState    = myL2Heap.allocate64ByteAligned<uint8_t>(gainSize);
         auto pOut          = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);
         auto pThreshold_dB = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);
         auto pKneeWidth_dB = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);
         auto pAttackCoeff  = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);
         auto pReleaseCoeff = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);

         void *pOutCn     = (void *) ddrBuffer;
         void *pGainCn    = (void *) (ddrBuffer + outputSize);
         void *pScratchCn = (void *) (pGainCn + gainSize);

         memset(pOutCn, 0, outputSize);
         TISP_limiter_init_cn<float>(pGainCn, numChannels);
         memset(pScratchCn, 0, scratchBufSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pScratch.get() && pGainState.get() && pOut.get() && pThreshold_dB.get() &&
             pKneeWidth_dB.get() &&
             pAttackCoeff.get() && pReleaseCoeff.get() && pOutCn && pGainCn) {

            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               // fill input buffer
               TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn.get(), prm[tpi].staticIn, numChannels, blockSize,
                                   pitchX, dataSize, testPatternString);

               // fill parameter buffers
               TI_fillBuffer_float(prm[tpi].testPattern, 0, pThreshold_dB.get(), prm[tpi].threshold_dB, numChannels, 1,
                                   0, dataSize, testPatternString);

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pKneeWidth_dB.get(), prm[tpi].kneeWidth_dB, numChannels, 1,
                                   0, dataSize, testPatternString);

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pAttackCoeff.get(), prm[tpi].attackCoeff, numChannels, 1, 0,
                                   dataSize, testPatternString);

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pReleaseCoeff.get(), prm[tpi].releaseCoeff, numChannels, 1,
                                   0, dataSize, testPatternString);
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = std::make_unique<TISP::AUDIOLIB::Limiter<float>>(
                (float *) pIn.get(), (float *) pScratch.get(), (float *) pOut.get(),
                (float *) pGainState.get(), (float *) pThreshold_dB.get(), (float *) pKneeWidth_dB.get(), (float *) pAttackCoeff.get(),
                (float *) pReleaseCoeff.get(), static_cast<uint32_t>(numChannels), static_cast<uint32_t>(blockSize),
                static_cast<uint32_t>(pitchX), static_cast<uint32_t>(pitchS), static_cast<uint32_t>(pitchY),
                static_cast<bool>(isInterleaved), errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            k0->setParams(static_cast<uint16_t>(TISP::AUDIOLIB::LimiterParamId::kBypass), &bypassMode,
                          sizeof(bypassMode), errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError setting bypass mode: %s\n", errorCtx.getMessage());
               return -1;
            }

            TI_profile_stop();

            // initialize set function parameters
            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;

            // set address for block to process
            kAddrMode    = TISP::kAddrModeInOut;
            kAddr.src[0] = (uint8_t *) pIn.get();
            kAddr.dst[0] = (uint8_t *) pOut.get();

            k0->setAddr(kAddr, kAddrMode);

            // Envelope persists across exec(), so reset it before each timed run.
            k0->cleanInternalBuffers();
            TI_profile_start(TI_PROFILE_KERNEL_OPT);

            // Execute the kernel
            k0->exec();

            TI_profile_stop();

            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */
            uint32_t k = 0;
            for (k = 0; k < 1; k++) {
               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get();
               kAddr.dst[0] = (uint8_t *) pOut.get();

               TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

               k0->setAddr(kAddr, kAddrMode);

               // Envelope persists across exec(), so reset it before each timed run.
               k0->cleanInternalBuffers();
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            // get output to L1D
            int16_t outSum   = 0;
            int8_t *pOutTemp = (int8_t *) pOut.get(); // treat output as bytes to be data type agnostic
            for (k = 0; k < blockSize * numChannels; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            // dummy store of outSum to insure that the compiler does not remove it.
            volatileSum = outSum;

            // set address for block to process
            kAddrMode    = TISP::kAddrModeInOut;
            kAddr.src[0] = (uint8_t *) pIn.get();
            kAddr.dst[0] = (uint8_t *) pOut.get();
            k0->setAddr(kAddr, kAddrMode);

            // run warm instruction cache test
            // Envelope persists across exec(), so reset it before each timed run.
            k0->cleanInternalBuffers();
            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

            // Execute the kernel
            k0->exec();

            TI_profile_stop();

            if (bypassMode != 0U) {
               // bypass copies the input to the output, leaving the limiter gain unapplied
               size_t   copyWidth  = isInterleaved ? numChannels : blockSize;
               size_t   copyHeight = isInterleaved ? blockSize : numChannels;
               uint8_t *pSrcCn     = (uint8_t *) pIn.get();
               uint8_t *pDstCn     = (uint8_t *) pOutCn;
               for (size_t i = 0; i < copyHeight; i++) {
                  memcpy(&pDstCn[i * pitchY], &pSrcCn[i * pitchX], copyWidth * dataSize);
               }
            }
            else {
               // Run C native implementation for comparison
               TISP_limiter_exec_cn<float>((float *) pIn.get(), (float *) pGainCn, (float *) pScratchCn,
                                           (float *) pOutCn, (float *) pThreshold_dB.get(),
                                           (float *) pKneeWidth_dB.get(), (float *) pAttackCoeff.get(),
                                           (float *) pReleaseCoeff.get(), numChannels, blockSize, pitchX, pitchS,
                                           pitchY, isInterleaved);
            }

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;

            status_nat_vs_opt &= TI_compare_mem_2D_float((void *) pOut.get(), (void *) pOutCn, 0, (double) 1e-2,
                                                         numChannels, blockSize, pitchY, dataSize);

            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;

               status_ref_vs_opt &= TI_compare_mem_2D_float((void *) pOut.get(), (void *) currPrm.staticOut, 0,
                                                            (double) 1e-2, numChannels, blockSize, pitchY, dataSize);

               // compare python vs natc
               status_ref_vs_nat = TI_TEST_KERNEL_PASS;

               status_ref_vs_nat &= TI_compare_mem_2D_float((void *) pOutCn, (void *) currPrm.staticOut, 0,
                                                            (double) 1e-2, numChannels, blockSize, pitchY, dataSize);
            }
            else {
               /* Set to pass since it wasn't supposed to run. */
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               status_ref_vs_nat = TI_TEST_KERNEL_PASS;
            }
            /* Set the 'fail' flag based on test vector comparison results */

            comparisonDone = 1;
            currentTestFail =
                ((status_nat_vs_opt == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) ||
                 (status_ref_vs_nat == TI_TEST_KERNEL_FAIL) || (comparisonDone == 0) || (currentTestFail == 1))
                    ? 1
                    : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            int32_t warmCycles = pProfile[3 * tpi + 1];

            sprintf(desc, "%s generated input | Num Channels=%d | BlockSize = %d | Interleaved Mode = %d\n",
                    testPatternString, currPrm.numChannels, currPrm.blockSize, currPrm.isInterleaved);

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV, "TISP_limiter, %d, %d, %zu, %zu, %d, %d, %lu, %d\n", testNum, currPrm.testPattern,
                    numChannels, blockSize, currPrm.isInterleaved, warmCycles, estCycles, !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.blockSize * currPrm.numChannels, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, blockSize=%d, numChannels=%d", AUDIOLIB_sizeof(currPrm.dataType),
                    currPrm.blockSize, currPrm.numChannels);
            TI_profile_skip_test(desc);
            // clear the counters between runs; normally handled by
            // TI_profile_add_test
            TI_profile_clear_run_stats();
         } // end of memory allocation successful?
      }    // end repetitions
   }       // end idat test cases

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if defined(_HOST_BUILD)
   printf("_HOST_BUILD is defined.\n");
#endif
   return TISP_limiter_d(&pProfile[0], 0);
}

/* Main call for individual test projects */
#ifdef __cplusplus
extern "C" {
#endif
int test_main_d()
{
   int fail = 1;

   uint32_t profile[256 * 3];

   /* TISP_TEST_init(); */

   fail = test_main(&profile[0]);

#if !defined(NO_PRINTF)
   if (fail == 0)
      printf("Test Pass!\n");
   else
      printf("Test Fail!\n");

   int i;
   for (i = 0; i < test_cases; i++) {
      printf("Test %4d: Cold Cycles = %8d, Warm Cycles = %8d, Warm Cycles WRB = "
             "%8d\n",
             i, profile[3 * i], profile[3 * i + 1], profile[3 * i + 2]);
   }
#endif
   return 0;
}

#ifdef __cplusplus
}
#endif
