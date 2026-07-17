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
#include "TISP_nullSink_idat.h"
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

std::unique_ptr<TISP::VirtualOp>
TISP_nullSink_createKernelObject(void *pIn, uint32_t dataType, TISP::ErrorCtxt &errorCtx)
{
   if (dataType == AUDIOLIB_FLOAT32) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<float>>((float *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_FLOAT64) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<double>>((double *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_INT8) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<int8_t>>((int8_t *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_UINT8) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<uint8_t>>((uint8_t *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_INT16) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<int16_t>>((int16_t *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_UINT16) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<uint16_t>>((uint16_t *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_INT32) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<int32_t>>((int32_t *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_UINT32) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<uint32_t>>((uint32_t *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_INT64) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<int64_t>>((int64_t *) pIn, errorCtx);
   }
   else if (dataType == AUDIOLIB_UINT64) {
      return std::make_unique<TISP::AUDIOLIB::Nullsink<uint64_t>>((uint64_t *) pIn, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_nullSink_createKernelObject",
                                        "unsupported data type for Nullsink kernel");
      return nullptr;
   }
}

int TISP_nullSink_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;

   TISP_nullSink_testParams_t *prm;
   TISP_nullSink_testParams_t  currPrm;
   TISP_nullSink_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_nullSink");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_nullSink.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "WidthIn, HeightIn, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      for (repCount = 0; repCount < numReps; repCount++) {

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */

         currentTestFail  = 0;
         size_t dataSize  = AUDIOLIB_sizeof(currPrm.dataType);
         size_t widthIn   = currPrm.widthIn;
         size_t heightIn  = currPrm.heightIn;
         size_t strideIn  = currPrm.strideInElements;
         size_t numBlocks = currPrm.numBlocks;
         size_t inputSize = numBlocks * heightIn * strideIn;

         auto pIn = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get()) {
            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               // fill input buffer
               TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn.get(), prm[tpi].staticIn0, widthIn, heightIn, strideIn,
                                   dataSize, testPatternString);
            }
            else {
               TI_fillBuffer(prm[tpi].testPattern, (uint8_t) 255, pIn.get(), prm[tpi].staticIn0, widthIn, heightIn,
                             strideIn, dataSize, testPatternString);
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_nullSink_createKernelObject(pIn.get(), currPrm.dataType, errorCtx);

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
               kAddrMode    = TISP::kAddrModeIn;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;

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
                  kAddrMode    = TISP::kAddrModeIn;
                  kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;

                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

                  k0->setAddr(kAddr, kAddrMode);

                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

                  // Execute the kernel
                  k0->exec();

                  TI_profile_stop();
               }
            }

            for (size_t n = 0; n < numBlocks; n++) {

               // set address for block to process
               kAddrMode    = TISP::kAddrModeIn;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            /* Set the 'fail' flag based on test vector comparison results */

            currentTestFail = (currentTestFail == 1) ? 1 : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString, AUDIOLIB_sizeof(currPrm.dataType));

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "TISP_nullSink, %d, %d,  "
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
   return TISP_nullSink_d(&pProfile[0], 0);
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
