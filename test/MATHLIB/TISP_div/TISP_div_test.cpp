// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include MATHLIB
#include <mathlib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <tisp.hpp>

// include test data for this kernel
#include "TISP_div_idat.h"

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) // 2MB KB
#endif

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

std::unique_ptr<TISP::VirtualOp> TISP_div_createKernelObject(size_t           size,
                                                             void            *pIn0,
                                                             void            *pIn1,
                                                             void            *pOut,
                                                             uint32_t         dataType,
                                                             TISP::ErrorCtxt &errorCtx)
{
   if (dataType == MATHLIB_FLOAT32) {
      return std::make_unique<TISP::MATHLIB::Div<float>>(size, (float *) pIn0, (float *) pIn1, (float *) pOut,
                                                         errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_div_createKernelObject", "unsupported data type for Div kernel");
      return nullptr;
   }
}

int TISP_div_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
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

   TISP_div_testParams_t *prm;
   TISP_div_testParams_t  currPrm;
   TISP_div_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_div");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_div.csv", "w+");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "Data size, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      for (repCount = 0; repCount < numReps; repCount++) {
         int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;

         currentTestFail   = 0;
         int32_t dataSize  = (currPrm.dataType == MATHLIB_FLOAT64) ? sizeof(double) : sizeof(float);
         size_t  size      = currPrm.length;
         size_t  numBlocks = currPrm.numBlocks;

         size_t inputSize  = numBlocks * size * dataSize;
         size_t outputSize = numBlocks * size * dataSize;

         auto pIn0 = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pIn1 = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pOut = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn0.get() && pIn1.get() && pOut.get()) {
            for (size_t n = 0; n < numBlocks; n++) {
               // fill input buffer
               TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn0.get() + n * size * dataSize,
                                   prm[tpi].staticIn0 + n * size * dataSize, size, 1, size, dataSize,
                                   testPatternString);
               TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn1.get() + n * size * dataSize,
                                   prm[tpi].staticIn1 + n * size * dataSize, size, 1, size, dataSize,
                                   testPatternString);
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto k0 = TISP_div_createKernelObject(static_cast<uint32_t>(size), pIn0.get(), pIn1.get(), pOut.get(),
                                                  currPrm.dataType, errorCtx);

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
               kAddr.src[0] = (uint8_t *) pIn0.get() + n * size * dataSize;
               kAddr.src[1] = (uint8_t *) pIn1.get() + n * size * dataSize;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * size * dataSize;

               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */
            for (k = 0; k < NUM_REPS; k++) {

               for (size_t n = 0; n < numBlocks; n++) {

                  // set address for block to process
                  kAddrMode    = TISP::kAddrModeInOut;
                  kAddr.src[0] = (uint8_t *) pIn0.get() + n * size * dataSize;
                  kAddr.src[1] = (uint8_t *) pIn1.get() + n * size * dataSize;
                  kAddr.dst[0] = (uint8_t *) pOut.get() + n * size * dataSize;

                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

                  k0->setAddr(kAddr, kAddrMode);

                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

                  // Execute the kernel
                  k0->exec();

                  TI_profile_stop();
               }
            }

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
               kAddr.src[0] = (uint8_t *) pIn0.get() + n * size * dataSize;
               kAddr.src[1] = (uint8_t *) pIn1.get() + n * size * dataSize;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * size * dataSize;

               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            outputSize = size;

            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               for (size_t n = 0; n < numBlocks; n++) {
                  status_ref_vs_opt &= TI_compare_mem_2D_float((int8_t *) pOut.get() + n * size * dataSize,
                                                               (int8_t *) currPrm.staticOut + n * size * dataSize,
                                                               10e-4, (double) powf(2, -10), size, 1, size, dataSize);
               }
            }
            else {
               /* Set to pass since it wasn't supposed to run. */
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
            }
            /* Set the 'fail' flag based on test vector comparison results */

            currentTestFail =
                ((status_ref_vs_opt == TI_TEST_KERNEL_FAIL) || (comparisonDone == 0) || (currentTestFail == 1)) ? 1 : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            sprintf(desc, "%s generated input | Data size=%ld ", testPatternString, dataSize);

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "TISP_div, %d, %d,  "
                    "%d, %lu, %lu, %.2f, %d\n",
                    testNum, currPrm.testPattern, size, archCycles, pProfile[3 * tpi + 1],
                    ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles), !currentTestFail);

            TI_profile_add_test(testNum++, size, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%ld, widthIn size=%d", dataSize, currPrm.length);
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
   return TISP_div_d(&pProfile[0], 0);
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
