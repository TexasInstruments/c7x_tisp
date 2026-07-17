// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include DSPLIB
#include <cstdint>
#include <dsplib.h>

// include test infrastructure provided by DSPLIB
#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <tisp.hpp>

// include test data for this kernel
#include "TISP_fir_cn.h"
#include "TISP_fir_idat.h"

#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB

#ifdef WIN32
#if defined(__C7504__) || defined(__C7524__)
int8_t l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t ddrBuffer[2048 * 1024];
#else
int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
int8_t ddrBuffer[2048 * 1024];
#endif
#else
#if defined(__C7504__) || defined(__C7524__)
__attribute__((section(".l2sramaux"), aligned(64))) int8_t  l2auxBuffer[TISP_L2_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t    ddrBuffer[2048 * 1024];
__attribute__((section(".l2sramData"), aligned(64))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];
#else
__attribute__((section(".msmcData"), aligned(64))) int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t  ddrBuffer[2048 * 1024];

#endif
#endif // WIN32

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

int DSPLIB_fir_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;

   DSPLIB_fir_testParams_t *prm;
   DSPLIB_fir_testParams_t  currPrm;
   DSPLIB_fir_getTestParams(&prm, &test_cases);

   TI_profile_init((char *) "DSPLIB_fir");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("DSPLIB_fir.csv", "w+");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, Bit Width, Parameters, "
                        "Data size, Batch size, Filter size, Arch cycles, Loki "
                        "cycles, Loki/Arch cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      for (repCount = 0; repCount < numReps; repCount++) {
         int32_t       status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t       status_ref_vs_opt = TI_TEST_KERNEL_FAIL;
         DSPLIB_STATUS status_init       = DSPLIB_SUCCESS;
         DSPLIB_STATUS status_opt        = DSPLIB_SUCCESS;
         DSPLIB_STATUS status_nat        = DSPLIB_SUCCESS;
         int32_t       status_ref_vs_nat = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;

         currentTestFail = 0;

         size_t inputSize  = currPrm.dataSize;
         size_t filterSize = currPrm.filterSize;
         size_t batchSize  = currPrm.batchSize;

         size_t inPitch     = currPrm.dataPitchIn;
         size_t filterPitch = currPrm.filterPitch;
         size_t outPitch    = currPrm.dataPitchOut;

         bool   enableNchCoefs = currPrm.enableNchCoefs;
         bool   enableMMA      = currPrm.enableMMA;
         bool   enableQ        = currPrm.enableQ; // for fixed point only
         size_t Q              = currPrm.Q;

         size_t numBlocks = currPrm.numBlocks;

         // TODO: determine state size
         size_t stateSize  = inputSize + filterSize - 1;
         size_t statePitch = stateSize * sizeof(DSPLIB_F32);

         // TODO: modify filter buffer size for MMA compute mode
         size_t mmaSize = c7x::element_count_of<c7x::float_vec>::value;

         size_t filterSizeMMA  = filterSize;
         size_t filterPitchMMA = filterPitch;
         size_t filterHeight   = (enableNchCoefs) ? batchSize : 1;
         size_t outputSize     = (inputSize); // or stateSize - filterSize + 1

         if (enableMMA) {
            filterSizeMMA  = filterSize + 3 * mmaSize - 1;
            filterPitchMMA = filterSizeMMA * sizeof(DSPLIB_F32);
         }

         /* void *pIn = (void *) TI_memalign(DSPLIB_L2DATA_ALIGNMENT, batchSize * inPitch * numBlocks); */

         auto pIn = myL2Heap.allocate64ByteAligned<uint8_t>(batchSize * inPitch * numBlocks);

         // TODO: allocate state buffer
         /* void *pState = (void *) TI_memalign(DSPLIB_L2DATA_ALIGNMENT, batchSize * statePitch); */

         auto pState = myL2Heap.allocate64ByteAligned<uint8_t>(batchSize * statePitch);

         /* void *pFilter; */
         /* if (enableMMA) { */

         /*    pFilter = (void *) TI_memalign(DSPLIB_L2DATA_ALIGNMENT, filterPitchMMA * filterHeight); */
         /* } */

         /* else { */
         /*    pFilter = (void *) TI_memalign(DSPLIB_L2DATA_ALIGNMENT, filterPitch * filterHeight); */
         /* } */

         auto pFilter = (enableMMA) ? (myL2Heap.allocate64ByteAligned<uint8_t>(filterPitchMMA * filterHeight))
                                    : (myL2Heap.allocate64ByteAligned<uint8_t>(filterPitch * filterHeight));

         auto  pOut   = myL2Heap.allocate64ByteAligned<uint8_t>(batchSize * outPitch * numBlocks);
         void *pOutCn = (void *) ddrBuffer;

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pState.get() && pOut.get() && pFilter.get() && pOutCn) {
            // fill input buffer
            TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn.get(), prm[tpi].staticIn, inputSize * numBlocks, batchSize,
                                (inPitch * numBlocks), sizeof(DSPLIB_F32), testPatternString);

            // initialize state values to 0
            memset(pState.get(), 0, statePitch * batchSize);

            // fill filter buffer
            if (enableMMA) {

               // initialize all values to 0 for right and left pad
               memset(pFilter.get(), 0, filterPitchMMA * filterHeight);

               /* fill relevant parts with coefficients */
               TI_fillBuffer_float(prm[tpi].testPattern, 0, (pFilter.get() + 2 * mmaSize * sizeof(DSPLIB_F32)),
                                   prm[tpi].filter, filterSize, filterHeight, filterPitchMMA, sizeof(DSPLIB_F32),
                                   testPatternString);
            }

            else {

               TI_fillBuffer_float(prm[tpi].testPattern, 0, pFilter.get(), prm[tpi].filter, filterSize, filterHeight,
                                   filterPitch, sizeof(DSPLIB_F32), testPatternString);
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = std::make_unique<TISP::DSPLIB::Fir<float>>(
                (float *) pIn.get(), (float *) pState.get(), (float *) pFilter.get(), (float *) pOut.get(),
                static_cast<uint32_t>(inputSize), static_cast<uint32_t>(filterSize), static_cast<uint32_t>(stateSize),
                static_cast<uint32_t>(batchSize), static_cast<uint32_t>(inPitch * numBlocks),
                static_cast<uint32_t>(statePitch), static_cast<uint32_t>(filterPitch),
                static_cast<uint32_t>(outPitch * numBlocks), static_cast<bool>(enableNchCoefs),
                static_cast<bool>(enableMMA), static_cast<bool>(enableQ), static_cast<uint32_t>(Q), errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            TI_profile_stop();

            // initialize set function parameters
            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;

            for (size_t n = 0; n < numBlocks; n++) {

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = pIn.get() + n * inputSize * sizeof(DSPLIB_F32);
               kAddr.src[1] = pState.get();
               kAddr.src[2] = pFilter.get();
               kAddr.dst[0] = pOut.get() + n * outputSize * sizeof(DSPLIB_F32);

               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            // initialize state values to 0
            memset(pState.get(), 0, statePitch * batchSize);

            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */

            for (k = 0; k < NUM_REPS; k++) {

               for (size_t n = 0; n < numBlocks; n++) {

                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

                  // set address for block to process
                  kAddrMode    = TISP::kAddrModeInOut;
                  kAddr.src[0] = pIn.get() + n * inputSize * sizeof(DSPLIB_F32);
                  kAddr.src[1] = pState.get();
                  kAddr.src[2] = pFilter.get();
                  kAddr.dst[0] = pOut.get() + n * outputSize * sizeof(DSPLIB_F32);

                  k0->setAddr(kAddr, kAddrMode);

                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

                  // Execute the kernel
                  k0->exec();

                  TI_profile_stop();
               }
            }

            // initialize state values to 0
            memset(pState.get(), 0, statePitch * batchSize);

            // get output to L1D
            int16_t outSum   = 0;
            int8_t *pOutTemp = (int8_t *) pOut.get(); // treat output as bytes to be data type agnostic
            for (k = 0; k < outputSize; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            // dummy store of outSum to insure that the compiler does not remove it.
            volatileSum = outSum;

            for (size_t n = 0; n < numBlocks; n++) {

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = pIn.get() + n * inputSize * sizeof(DSPLIB_F32);
               kAddr.src[1] = pState.get();
               kAddr.src[2] = pFilter.get();
               kAddr.dst[0] = pOut.get() + n * outputSize * sizeof(DSPLIB_F32);

               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            // initialize state values to 0
            memset(pState.get(), 0, statePitch * batchSize);

            // Execute natural C code
            uint32_t filterOffsetCn = 0;

            if (enableMMA) {

               filterOffsetCn = 2 * mmaSize * sizeof(DSPLIB_F32);
            }

            outputSize = inputSize;

            for (size_t n = 0; n < numBlocks; n++) {
               DSPLIB_fir_cn((float *) ((int8_t *) pIn.get() + n * inputSize * sizeof(DSPLIB_F32)),
                             (float *) pState.get(), (float *) ((int8_t *) pFilter.get() + filterOffsetCn),
                             (float *) ((int8_t *) pOutCn + n * outputSize * sizeof(DSPLIB_F32)), inputSize, stateSize,
                             filterSize, outputSize, batchSize, (inPitch * numBlocks), statePitch, filterPitch,
                             (outPitch * numBlocks), enableNchCoefs, enableMMA, enableQ, Q);
            }

            status_nat_vs_opt = TI_compare_mem_2D_float((void *) pOut.get(), (void *) pOutCn, 0, (double) powf(2, -10),
                                                        (outputSize * numBlocks), batchSize, (outPitch * numBlocks),
                                                        sizeof(DSPLIB_F32));

            status_ref_vs_nat = TI_compare_mem_2D_float((void *) pOutCn, (void *) currPrm.staticOut, 0,
                                                        (double) powf(2, -10), (outputSize * numBlocks), batchSize,
                                                        (outPitch * numBlocks), sizeof(DSPLIB_F32));
            comparisonDone    = 1;

            if (currPrm.staticOut != nullptr) {

               status_ref_vs_opt = TI_compare_mem_2D_float((void *) pOut.get(), (void *) currPrm.staticOut, 0,
                                                           (double) powf(2, -10), (outputSize * numBlocks), batchSize,
                                                           (outPitch * numBlocks), sizeof(DSPLIB_F32));

               comparisonDone = 1;
            }
            else {
               /* Set to pass since it wasn't supposed to run. */
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
            }

            /* Set the 'fail' flag based on test vector comparison results */
            currentTestFail =
                ((status_nat_vs_opt == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) ||
                 (status_ref_vs_nat == TI_TEST_KERNEL_FAIL) || (status_init != DSPLIB_SUCCESS) ||
                 (status_opt != DSPLIB_SUCCESS) || (status_nat != DSPLIB_SUCCESS) || (comparisonDone == 0) ||
                 (currentTestFail == 1))
                    ? 1
                    : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            sprintf(desc, "%s generated input | Data size=%d, Batch size=%d, Filter size=%d", testPatternString,
                    currPrm.dataSize, currPrm.batchSize, currPrm.filterSize);

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "FIR, %d, %d, %d, Data size:%d Batch size: %d Filter size:%d, "
                    "%d, %d, %d, %ld, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, DSPLIB_sizeof(currPrm.dataType) * 8, currPrm.dataSize,
                    currPrm.batchSize, currPrm.filterSize, currPrm.dataSize, currPrm.batchSize, currPrm.filterSize,
                    archCycles, pProfile[3 * tpi + 1],
                    ((DSPLIB_F32) cycles[TI_PROFILE_KERNEL_OPT]) / ((DSPLIB_F32) archCycles), !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.dataSize * currPrm.batchSize * currPrm.filterSize, 0, 0,
                                currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, Batch size=%d, Filter size=%d", currPrm.dataSize, currPrm.batchSize,
                    currPrm.filterSize);
            TI_profile_skip_test(desc);
            // clear the counters between runs; normally handled by
            // TI_profile_add_test
            TI_profile_clear_run_stats();
         } // end of memory allocation successful?

      } // end repetitions
   }    // end idat test cases

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("_HOST_BUILD is defined.\n");
#endif

   return DSPLIB_fir_d(&pProfile[0], 0);
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
   return fail;
}

#ifdef __cplusplus
}
#endif
