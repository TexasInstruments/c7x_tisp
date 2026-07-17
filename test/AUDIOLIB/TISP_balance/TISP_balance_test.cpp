// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include AUDIOLIB
#include <audiolib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <tisp.hpp>

// include test data for this kernel
#include "TISP_balance_cn.h"
#include "TISP_balance_idat.h"
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

float currentGainL;
float currentGainR;

int TISP_balance_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;
   uint64_t estCycles  = 0;

   TISP_balance_testParams_t *prm;
   TISP_balance_testParams_t  currPrm;
   TISP_balance_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_balance");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_balance.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "WidthIn, HeightIn, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      float   balance       = currPrm.balance;
      int32_t samplingRate  = currPrm.samplingRate;
      float   smoothingTime = currPrm.smoothingTime;
      size_t  numBlocks     = currPrm.numBlocks;

      for (repCount = 0; repCount < numReps; repCount++) {
         int32_t status_nat_vs_opt_left  = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_opt_left  = TI_TEST_KERNEL_FAIL;
         int32_t status_nat_vs_opt_right = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_opt_right = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;

         currentTestFail          = 0;
         size_t dataSize          = AUDIOLIB_sizeof(currPrm.dataType);
         size_t widthIn           = currPrm.widthIn;
         size_t heightIn          = currPrm.heightIn;
         size_t strideIn          = currPrm.strideInElements;
         size_t strideOut         = currPrm.strideOutElements;
         size_t strideInElements  = strideIn / dataSize;
         size_t strideOutElements = strideOut / dataSize;

         size_t inputSize  = numBlocks * heightIn * strideIn;
         size_t outputSize = numBlocks * heightIn * strideOut;

         auto pInL  = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pOutL = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);
         auto pInR  = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pOutR = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);

         void *pOutCnL = (void *) ddrBuffer;
         void *pOutCnR = (void *) (ddrBuffer + outputSize);
         memset(pOutCnL, 0, outputSize);
         memset(pOutCnR, 0, outputSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pInL.get() && pInR.get() && pOutL.get() && pOutR.get() && pOutCnL && pOutCnR) {
            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               for (size_t n = 0; n < numBlocks; n++) {
                  // fill input buffer
                  TI_fillBuffer_float(prm[tpi].testPattern, 0,
                                      (void *) ((int8_t *) pInL.get() + n * heightIn * strideIn),
                                      (void *) ((float *) prm[tpi].staticInLeft + (n * widthIn * heightIn)), widthIn,
                                      heightIn, strideIn, AUDIOLIB_sizeof(currPrm.dataType), testPatternString);

                  TI_fillBuffer_float(prm[tpi].testPattern, 0,
                                      (void *) ((int8_t *) pInR.get() + n * heightIn * strideIn),
                                      (void *) ((float *) prm[tpi].staticInRight + (n * widthIn * heightIn)), widthIn,
                                      heightIn, strideIn, AUDIOLIB_sizeof(currPrm.dataType), testPatternString);
               }
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = std::make_unique<TISP::AUDIOLIB::Balance<float>>(
                (float *) pInL.get(), (float *) pInR.get(), (float *) pOutL.get(), (float *) pOutR.get(),
                static_cast<uint32_t>(widthIn), static_cast<uint32_t>(heightIn), static_cast<int32_t>(strideIn),
                static_cast<int32_t>(strideOut), static_cast<float>(balance), static_cast<float>(smoothingTime),
                static_cast<int32_t>(samplingRate), errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            TI_profile_stop();

            // initialize set function parameters
            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;

            // set address for block to process
            kAddrMode    = TISP::kAddrModeInOut;
            kAddr.src[0] = (int8_t *) pInL.get();
            kAddr.src[1] = (int8_t *) pInR.get();
            kAddr.dst[0] = (int8_t *) pOutL.get();
            kAddr.dst[1] = (int8_t *) pOutR.get();

            k0->setAddr(kAddr, kAddrMode);

            TI_profile_start(TI_PROFILE_KERNEL_OPT);

            // Execute the kernel
            k0->exec();

            TI_profile_stop();

            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */

            for (size_t n = 0; n < numBlocks - 2; n++) {

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (int8_t *) pInL.get() + (n + 1) * heightIn * strideIn;
               kAddr.src[1] = (int8_t *) pInR.get() + (n + 1) * heightIn * strideIn;
               kAddr.dst[0] = (int8_t *) pOutL.get() + (n + 1) * heightIn * strideOut;
               kAddr.dst[1] = (int8_t *) pOutR.get() + (n + 1) * heightIn * strideOut;

               TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            // get output to L1D
            int16_t outSum    = 0;
            int8_t *pOutTempL = (int8_t *) pOutL.get(); // treat output as bytes to be data type agnostic
            int8_t *pOutTempR = (int8_t *) pOutR.get(); // treat output as bytes to be data type agnostic
            for (k = 0; k < outputSize; k++) {
               outSum += *pOutTempL;
               pOutTempL++;
               outSum += *pOutTempR;
               pOutTempR++;
            }

            // dummy store of outSum to insure that the compiler does not remove it.
            volatileSum = outSum;

            if (numBlocks != 1) {
               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (int8_t *) pInL.get() + (numBlocks - 1) * heightIn * strideIn;
               kAddr.dst[0] = (int8_t *) pOutL.get() + (numBlocks - 1) * heightIn * strideOut;
               kAddr.src[1] = (int8_t *) pInR.get() + (numBlocks - 1) * heightIn * strideIn;
               kAddr.dst[1] = (int8_t *) pOutR.get() + (numBlocks - 1) * heightIn * strideOut;
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            outputSize = widthIn;

            currentGainL = 0.0f;
            currentGainR = 0.0f;

            for (size_t n = 0; n < numBlocks; n++) {
               AUDIOLIB_balance_cn((void *) ((int8_t *) pInL.get() + n * heightIn * strideIn),
                                   (void *) ((int8_t *) pInR.get() + n * heightIn * strideIn),
                                   (void *) ((int8_t *) pOutCnL + n * heightIn * strideOut),
                                   (void *) ((int8_t *) pOutCnR + n * heightIn * strideOut), currPrm.dataType, widthIn,
                                   heightIn, strideInElements, strideOutElements, balance, samplingRate, smoothingTime);
            }

            status_nat_vs_opt_left  = TI_TEST_KERNEL_PASS;
            status_nat_vs_opt_right = TI_TEST_KERNEL_PASS;
            for (size_t n = 0; n < numBlocks; n++) {
               status_nat_vs_opt_left &=
                   TI_compare_mem_2D_float((void *) ((int8_t *) pOutL.get() + n * heightIn * strideOut),
                                           (void *) ((int8_t *) pOutCnL + n * heightIn * strideOut), 0,
                                           (double) powf(2, -10), widthIn, heightIn, strideOut, dataSize);
               status_nat_vs_opt_right &=
                   TI_compare_mem_2D_float((void *) ((int8_t *) pOutR.get() + n * heightIn * strideOut),
                                           (void *) ((int8_t *) pOutCnR + n * heightIn * strideOut), 0,
                                           (double) powf(2, -10), widthIn, heightIn, strideOut, dataSize);
            }

            comparisonDone = 1;
            if (currPrm.staticOutLeft != nullptr && currPrm.staticOutRight != nullptr) {
               status_ref_vs_opt_left  = TI_TEST_KERNEL_PASS;
               status_ref_vs_opt_right = TI_TEST_KERNEL_PASS;
               if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                  for (size_t n = 0; n < numBlocks; n++) {
                     status_ref_vs_opt_left &= TI_compare_mem_2DDualStride_float(
                         (void *) ((int8_t *) pOutL.get() + n * heightIn * strideOut),
                         (void *) ((int8_t *) currPrm.staticOutLeft + n * widthIn * heightIn * dataSize), 0,
                         (double) powf(2, -10), widthIn, heightIn, strideOut, widthIn * dataSize, dataSize);
                     status_ref_vs_opt_right &= TI_compare_mem_2DDualStride_float(
                         (void *) ((int8_t *) pOutR.get() + n * heightIn * strideOut),
                         (void *) ((int8_t *) currPrm.staticOutRight + n * widthIn * heightIn * dataSize), 0,
                         (double) powf(2, -10), widthIn, heightIn, strideOut, widthIn * dataSize, dataSize);
                  }
               }
            }
            else {
               /* Set to pass since it wasn't supposed to run. */
               status_ref_vs_opt_left  = TI_TEST_KERNEL_PASS;
               status_ref_vs_opt_right = TI_TEST_KERNEL_PASS;
            }
            /* Set the 'fail' flag based on test vector comparison results */

            currentTestFail =
                ((status_nat_vs_opt_left == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt_left == TI_TEST_KERNEL_FAIL) ||
                 (status_nat_vs_opt_right == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt_right == TI_TEST_KERNEL_FAIL) ||
                 (comparisonDone == 0) || (currentTestFail == 1))
                    ? 1
                    : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString, AUDIOLIB_sizeof(currPrm.dataType));

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "TISP_balance, %d, %d,  "
                    "%d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.widthIn, currPrm.heightIn, archCycles, pProfile[3 * tpi + 1],
                    ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles), !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.widthIn * currPrm.heightIn, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", AUDIOLIB_sizeof(currPrm.dataType),
                    currPrm.widthIn, currPrm.heightIn);
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
#if !defined(_HOST_BUILD)
   printf("_HOST_BUILD is defined.\n");
#endif
   return TISP_balance_d(&pProfile[0], 0);
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

   return 1;
}

#ifdef __cplusplus
}
#endif
