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
#include "TISP_downwardExpander_cn.h"
#include "TISP_downwardExpander_idat.h"
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

// Output passes within 1e-3 in log10 (0.02 dB) or 1e-6 absolute; the gain state (dB) within 0.02 dB
#define TISP_DOWNWARDEXPANDER_OUT_LOG10_TOL (1e-3)
#define TISP_DOWNWARDEXPANDER_OUT_ABS_TOL   (1e-6)
#define TISP_DOWNWARDEXPANDER_GAIN_ABS_TOL  (2e-2)

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

int TISP_downwardExpander_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t testNum;
   uint64_t estCycles  = 0;

   TISP_downwardExpander_testParams_t *prm;
   TISP_downwardExpander_testParams_t  currPrm;
   TISP_downwardExpander_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_downwardExpander");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_downwardExpander.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "Num channels, Block size, Warm cycles, Perf est cycles, Pass/Fail\n");

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

         size_t   pitchX     = currPrm.pitchX;
         size_t   pitchS     = currPrm.pitchS;
         size_t   pitchY     = currPrm.pitchY;
         uint32_t bypassMode = currPrm.bypassMode;
         uint32_t numBlocks  = currPrm.numBlocks;

         size_t inputSize, gainSize, outputSize, scratchBufSize, scratchCnSize;
         size_t paramSize;

         inputSize      = blockSize * pitchX;
         gainSize       = numChannels * dataSize;
         outputSize     = blockSize * pitchY;
         scratchBufSize = blockSize * numChannels * dataSize;
         scratchCnSize  = blockSize * pitchS;
         paramSize      = numChannels * dataSize;

         auto pIn           = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pScratch      = myL2Heap.allocate64ByteAligned<uint8_t>(scratchBufSize);
         auto pGainState    = myL2Heap.allocate64ByteAligned<uint8_t>(gainSize);
         auto pHoldState    = myL2Heap.allocate64ByteAligned<uint8_t>(gainSize);
         auto pOut          = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);
         auto pThreshold_dB = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);
         auto pRatio        = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);
         auto pKneeWidth_dB = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);
         auto pAttackCoeff  = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);
         auto pReleaseCoeff = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);
         auto pHoldSamples  = myL2Heap.allocate64ByteAligned<uint8_t>(paramSize);

         void *pOutCn     = (void *) ddrBuffer;
         void *pGainCn    = (void *) (ddrBuffer + outputSize);
         void *pHoldCn    = (void *) (pGainCn + gainSize);
         void *pScratchCn = (void *) (pHoldCn + gainSize);

         memset(pOutCn, 0, outputSize);
         TISP_downwardExpander_init_cn<float>(pGainCn, pHoldCn, numChannels);
         memset(pScratchCn, 0, scratchCnSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pScratch.get() && pGainState.get() && pHoldState.get() && pOut.get() &&
             pThreshold_dB.get() && pRatio.get() && pKneeWidth_dB.get() && pAttackCoeff.get() &&
             pReleaseCoeff.get() && pHoldSamples.get() && pOutCn && pGainCn) {

            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               // fill input buffer
               TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn.get(), prm[tpi].staticIn, numChannels, blockSize,
                                   pitchX, dataSize, testPatternString);

               // fill parameter buffers
               TI_fillBuffer_float(prm[tpi].testPattern, 0, pThreshold_dB.get(), prm[tpi].threshold_dB, numChannels, 1,
                                   0, dataSize, testPatternString);

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pRatio.get(), prm[tpi].ratio, numChannels, 1, 0,
                                   dataSize, testPatternString);

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pKneeWidth_dB.get(), prm[tpi].kneeWidth_dB, numChannels, 1,
                                   0, dataSize, testPatternString);

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pAttackCoeff.get(), prm[tpi].attackCoeff, numChannels, 1, 0,
                                   dataSize, testPatternString);

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pReleaseCoeff.get(), prm[tpi].releaseCoeff, numChannels, 1,
                                   0, dataSize, testPatternString);

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pHoldSamples.get(), prm[tpi].holdSamples, numChannels, 1,
                                   0, dataSize, testPatternString);
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            const TISP::TISP_BufDesc2D inDesc  = {pIn.get(), TISP::Types::kFloat32, numChannels, blockSize,
                                                  static_cast<size_t>(pitchX), 1U};
            const TISP::TISP_BufDesc2D outDesc = {pOut.get(), TISP::Types::kFloat32, numChannels, blockSize,
                                                  static_cast<size_t>(pitchY), 1U};
            // state 0: smoothed gain, state 1: hold counter
            const TISP::TISP_BufDesc2D stateDescs[2] = {
                {pGainState.get(), TISP::Types::kFloat32, numChannels, 1U, numChannels * sizeof(float), 0U},
                {pHoldState.get(), TISP::Types::kFloat32, numChannels, 1U, numChannels * sizeof(float), 0U}};

            const TISP::TISP_BufDescList inputs  = {&inDesc, 1U};
            const TISP::TISP_BufDescList outputs = {&outDesc, 1U};
            const TISP::TISP_BufDescList states  = {stateDescs, 2U};

            TISP::AUDIOLIB::DownwardExpanderTunableParams<float> defaultParams;
            defaultParams.pThresholdDb  = reinterpret_cast<float *>(pThreshold_dB.get());
            defaultParams.pRatio        = reinterpret_cast<float *>(pRatio.get());
            defaultParams.pKneeWidthDb  = reinterpret_cast<float *>(pKneeWidth_dB.get());
            defaultParams.pAttackCoeff  = reinterpret_cast<float *>(pAttackCoeff.get());
            defaultParams.pReleaseCoeff = reinterpret_cast<float *>(pReleaseCoeff.get());
            defaultParams.pHoldSamples  = reinterpret_cast<float *>(pHoldSamples.get());

            TISP::ErrorCtxt errorCtx;
            auto            k0 = std::make_unique<TISP::AUDIOLIB::DownwardExpander<float>>(
                &inputs, &outputs, &states, pScratch.get(), scratchBufSize, &defaultParams, "DownwardExpander", 0,
                errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            k0->setParams(static_cast<uint16_t>(TISP::AUDIOLIB::DownwardExpanderParamId::kBypassMode), &bypassMode,
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

            // Verification: run all numBlocks blocks back to back from rest, carrying the state across exec().
            // The reference holds numBlocks * blockSize packed rows; the node and natc buffers are pitched.
            size_t refRowBytes   = numChannels * dataSize;
            size_t refBlockBytes = blockSize * refRowBytes;
            bool   hasRef        = (currPrm.staticOut != nullptr);

            k0->cleanInternalBuffers();
            TISP_downwardExpander_init_cn<float>(pGainCn, pHoldCn, numChannels);

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            status_ref_vs_opt = TI_TEST_KERNEL_PASS;
            status_ref_vs_nat = TI_TEST_KERNEL_PASS;

            for (uint32_t blk = 0; blk < numBlocks; blk++) {
               if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                  void *pStaticIn = (void *) ((uint8_t *) currPrm.staticIn + blk * refBlockBytes);
                  TI_fillBuffer_float(currPrm.testPattern, 0, pIn.get(), pStaticIn, numChannels, blockSize, pitchX,
                                      dataSize, testPatternString);
               }

               k0->exec();

               if (bypassMode != 0U) {
                  // bypass copies the input to the output, leaving the expander gain unapplied
                  uint8_t *pSrcCn = (uint8_t *) pIn.get();
                  uint8_t *pDstCn = (uint8_t *) pOutCn;
                  for (size_t i = 0; i < blockSize; i++) {
                     memcpy(&pDstCn[i * pitchY], &pSrcCn[i * pitchX], numChannels * dataSize);
                  }
               }
               else {
                  // Run C native implementation for comparison
                  TISP_downwardExpander_exec_cn<float>(
                      (float *) pIn.get(), (float *) pGainCn, (float *) pHoldCn, (float *) pScratchCn, (float *) pOutCn,
                      (float *) pThreshold_dB.get(), (float *) pRatio.get(), (float *) pKneeWidth_dB.get(),
                      (float *) pAttackCoeff.get(), (float *) pReleaseCoeff.get(), (float *) pHoldSamples.get(),
                      numChannels, blockSize, pitchX, pitchS, pitchY);
               }

               status_nat_vs_opt &= TI_compare_mem_2D_float((void *) pOut.get(), (void *) pOutCn,
                                                            TISP_DOWNWARDEXPANDER_OUT_LOG10_TOL,
                                                            TISP_DOWNWARDEXPANDER_OUT_ABS_TOL, numChannels, blockSize,
                                                            pitchY, dataSize);

               if (hasRef) {
                  void *pStaticOut = (void *) ((uint8_t *) currPrm.staticOut + blk * refBlockBytes);

                  status_ref_vs_opt &= TI_compare_mem_2DDualStride_float(
                      (void *) pOut.get(), pStaticOut, TISP_DOWNWARDEXPANDER_OUT_LOG10_TOL,
                      TISP_DOWNWARDEXPANDER_OUT_ABS_TOL, numChannels, blockSize, pitchY, refRowBytes, dataSize);

                  // compare python vs natc
                  status_ref_vs_nat &= TI_compare_mem_2DDualStride_float(
                      pOutCn, pStaticOut, TISP_DOWNWARDEXPANDER_OUT_LOG10_TOL, TISP_DOWNWARDEXPANDER_OUT_ABS_TOL,
                      numChannels, blockSize, pitchY, refRowBytes, dataSize);
               }
            }

            // The envelope and hold counter carried to the next exec() must match too; the counter exactly
            status_nat_vs_opt &= TI_compare_mem_2D_float((void *) pGainState.get(), (void *) pGainCn, 0,
                                                         TISP_DOWNWARDEXPANDER_GAIN_ABS_TOL, numChannels, 1, 0,
                                                         dataSize);
            status_nat_vs_opt &= TI_compare_mem_2D_float((void *) pHoldState.get(), (void *) pHoldCn, 0, 0,
                                                         numChannels, 1, 0, dataSize);

            if (hasRef && (currPrm.staticGainOut != nullptr)) {
               status_ref_vs_opt &= TI_compare_mem_2D_float((void *) pGainState.get(), currPrm.staticGainOut, 0,
                                                            TISP_DOWNWARDEXPANDER_GAIN_ABS_TOL, numChannels, 1, 0,
                                                            dataSize);
               status_ref_vs_nat &= TI_compare_mem_2D_float(pGainCn, currPrm.staticGainOut, 0,
                                                            TISP_DOWNWARDEXPANDER_GAIN_ABS_TOL, numChannels, 1, 0,
                                                            dataSize);
            }

            if (hasRef && (currPrm.staticHoldOut != nullptr)) {
               status_ref_vs_opt &= TI_compare_mem_2D_float((void *) pHoldState.get(), currPrm.staticHoldOut, 0, 0,
                                                            numChannels, 1, 0, dataSize);
               status_ref_vs_nat &= TI_compare_mem_2D_float(pHoldCn, currPrm.staticHoldOut, 0, 0, numChannels, 1, 0,
                                                            dataSize);
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

            sprintf(desc, "%s generated input | Num Channels=%d | BlockSize = %d\n",
                    testPatternString, currPrm.numChannels, currPrm.blockSize);

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV, "TISP_downwardExpander, %d, %d, %zu, %zu, %d, %lu, %d\n", testNum, currPrm.testPattern,
                    numChannels, blockSize, warmCycles, estCycles, !currentTestFail);

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
   return TISP_downwardExpander_d(&pProfile[0], 0);
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
