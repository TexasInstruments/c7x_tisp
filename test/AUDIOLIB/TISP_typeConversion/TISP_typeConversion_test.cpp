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
#include "TISP_typeConversion_cn.h"
#include "TISP_typeConversion_idat.h"
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

std::unique_ptr<TISP::VirtualOp> TISP_typeConversion_createKernelObject(void            *pIn,
                                                                        void            *pOut,
                                                                        uint32_t         indataType,
                                                                        uint32_t         outdataType,
                                                                        int32_t          widthIn,
                                                                        int32_t          heightIn,
                                                                        int32_t          strideIn,
                                                                        int32_t          strideOut,
                                                                        uint8_t          testQ31,
                                                                        TISP::ErrorCtxt &errorCtx)
{
   if (indataType == AUDIOLIB_INT16) {
      if (outdataType == AUDIOLIB_FLOAT32) {
         return std::make_unique<TISP::AUDIOLIB::TypeConversion<int16_t, float>>(
             (int16_t *) pIn, (float *) pOut, widthIn, heightIn, strideIn, strideOut, testQ31, errorCtx);
      }
      else if (outdataType == AUDIOLIB_FLOAT64) {
         return std::make_unique<TISP::AUDIOLIB::TypeConversion<int16_t, double>>(
             (int16_t *) pIn, (double *) pOut, widthIn, heightIn, strideIn, strideOut, testQ31, errorCtx);
      }
   }
   else if (outdataType == AUDIOLIB_INT16) {
      if (indataType == AUDIOLIB_FLOAT32) {
         return std::make_unique<TISP::AUDIOLIB::TypeConversion<float, int16_t>>(
             (float *) pIn, (int16_t *) pOut, widthIn, heightIn, strideIn, strideOut, testQ31, errorCtx);
      }
      else if (indataType == AUDIOLIB_FLOAT64) {
         return std::make_unique<TISP::AUDIOLIB::TypeConversion<double, int16_t>>(
             (double *) pIn, (int16_t *) pOut, widthIn, heightIn, strideIn, strideOut, testQ31, errorCtx);
      }
   }
   else if (indataType == AUDIOLIB_INT32) {
      if (outdataType == AUDIOLIB_FLOAT32) {
         return std::make_unique<TISP::AUDIOLIB::TypeConversion<int32_t, float>>(
             (int32_t *) pIn, (float *) pOut, widthIn, heightIn, strideIn, strideOut, testQ31, errorCtx);
      }
      else if (outdataType == AUDIOLIB_FLOAT64) {
         return std::make_unique<TISP::AUDIOLIB::TypeConversion<int32_t, double>>(
             (int32_t *) pIn, (double *) pOut, widthIn, heightIn, strideIn, strideOut, testQ31, errorCtx);
      }
   }
   else if (outdataType == AUDIOLIB_INT32) {
      if (indataType == AUDIOLIB_FLOAT32) {
         return std::make_unique<TISP::AUDIOLIB::TypeConversion<float, int32_t>>(
             (float *) pIn, (int32_t *) pOut, widthIn, heightIn, strideIn, strideOut, testQ31, errorCtx);
      }
      else if (indataType == AUDIOLIB_FLOAT64) {
         return std::make_unique<TISP::AUDIOLIB::TypeConversion<double, int32_t>>(
             (double *) pIn, (int32_t *) pOut, widthIn, heightIn, strideIn, strideOut, testQ31, errorCtx);
      }
   }
   else {
      errorCtx.setInvalidParameterError("TISP_typeConversion_createKernelObject",
                                        "unsupported data type for TypeConversion kernel");
      return nullptr;
   }
}

int TISP_typeConversion_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
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

   TISP_typeConversion_testParams_t *prm;
   TISP_typeConversion_testParams_t  currPrm;
   TISP_typeConversion_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_typeConversion");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_typeConversion.csv", "w");
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
         int32_t status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;

         currentTestFail          = 0;
         size_t inDataSize        = AUDIOLIB_sizeof(currPrm.indataType);
         size_t outDataSize       = AUDIOLIB_sizeof(currPrm.outdataType);
         size_t widthIn           = currPrm.widthIn;
         size_t heightIn          = currPrm.heightIn;
         size_t strideIn          = currPrm.strideInElements;
         size_t strideOut         = currPrm.strideOutElements;
         size_t numBlocks         = currPrm.numBlocks;
         size_t inputSize         = numBlocks * heightIn * strideIn;
         size_t outputSize        = numBlocks * heightIn * strideOut;
         size_t strideInElements  = strideIn / inDataSize;
         size_t strideOutElements = strideOut / outDataSize;
         size_t testQ31           = currPrm.testQ31;

         auto pIn  = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pOut = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);

         void *pOutCn = (void *) ddrBuffer;
         memset(pOutCn, 0, outputSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pOut.get() && pOutCn) {
            for (size_t n = 0; n < numBlocks; n++) {
               if (currPrm.indataType == AUDIOLIB_FLOAT32 || currPrm.indataType == AUDIOLIB_FLOAT64) {
                  // fill input buffer
                  TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn.get() + n * heightIn * strideIn,
                                      prm[tpi].staticIn0 + n * heightIn * widthIn * inDataSize, widthIn, heightIn,
                                      strideIn, inDataSize, testPatternString);
               }
               else {
                  TI_fillBuffer(prm[tpi].testPattern, (uint8_t) 255, pIn.get() + n * heightIn * strideIn,
                                prm[tpi].staticIn0 + n * heightIn * widthIn * inDataSize, widthIn, heightIn, strideIn,
                                inDataSize, testPatternString);
               }
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_typeConversion_createKernelObject(
                           pIn.get(), pOut.get(), currPrm.indataType, currPrm.outdataType, static_cast<int32_t>(widthIn),
                           static_cast<int32_t>(heightIn), static_cast<int32_t>(strideIn), static_cast<int32_t>(strideOut),
                           static_cast<uint8_t>(testQ31), errorCtx);

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
               kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightIn * strideOut;

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
                  kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;
                  kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightIn * strideOut;

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

               //    // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightIn * strideOut;
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            outputSize = widthIn;

            for (size_t n = 0; n < numBlocks; n++) {
               AUDIOLIB_typeConversion_cn(pIn.get() + n * heightIn * strideIn, pOutCn + n * heightIn * strideOut,
                                          currPrm.indataType, currPrm.outdataType, widthIn, heightIn, strideInElements,
                                          strideOutElements, testQ31);
            }

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            for (size_t n = 0; n < numBlocks; n++) {
               if (currPrm.outdataType == AUDIOLIB_FLOAT32 || currPrm.outdataType == AUDIOLIB_FLOAT64) {
                  status_nat_vs_opt &= TI_compare_mem_2D_float(
                      (void *) pOut.get() + n * heightIn * strideOut, (void *) pOutCn + n * heightIn * strideOut, 0.001,
                      (double) powf(2, -10), widthIn, heightIn, strideOut, AUDIOLIB_sizeof(currPrm.outdataType));
               }
               else {
                  status_nat_vs_opt &= TI_compare_mem_2D((void *) pOut.get() + n * heightIn * strideOut,
                                                         (void *) pOutCn + n * heightIn * strideOut, 0, 0, widthIn,
                                                         heightIn, strideOut, AUDIOLIB_sizeof(currPrm.outdataType));
               }
            }

            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               for (size_t n = 0; n < numBlocks; n++) {
                  if (currPrm.outdataType == AUDIOLIB_FLOAT32 || currPrm.outdataType == AUDIOLIB_FLOAT64) {
                     status_ref_vs_opt &= TI_compare_mem_2DDualStride_float(
                         (void *) pOut.get() + n * heightIn * strideOut,
                         (void *) currPrm.staticOut + n * heightIn * widthIn * outDataSize, 0.001,
                         (double) powf(2, -10), widthIn, heightIn, strideOut, widthIn * outDataSize, outDataSize);
                  }
                  else {
                     status_ref_vs_opt &= TI_compare_mem_2DDualStride(
                         (void *) pOut.get() + n * heightIn * strideOut,
                         (void *) currPrm.staticOut + n * heightIn * widthIn * outDataSize, 0, 0, widthIn, heightIn,
                         strideOut, widthIn * outDataSize, outDataSize);
                  }
               }
            }
            else {
               /* Set to pass since it wasn't supposed to run. */
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
            }
            /* Set the 'fail' flag based on test vector comparison results */

            currentTestFail =
                ((status_nat_vs_opt == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) ||
                 (comparisonDone == 0) || (currentTestFail == 1))
                    ? 1
                    : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString, AUDIOLIB_sizeof(currPrm.indataType));

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "TISP_typeConversion, %d, %d,  "
                    "%d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.widthIn, currPrm.heightIn, archCycles, pProfile[3 * tpi + 1],
                    ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles), !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.widthIn * currPrm.heightIn, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", AUDIOLIB_sizeof(currPrm.indataType),
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
   return TISP_typeConversion_d(&pProfile[0], 0);
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
