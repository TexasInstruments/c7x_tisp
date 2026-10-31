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
#include "TISP_tableLookup_cn.h"
#include "TISP_tableLookup_idat.h"
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

#define AUDIOLIB_ROW_STRIDE(x, y) (((x + y) / y) * y)

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

template <typename TIn, typename TOut>
std::unique_ptr<TISP::VirtualOp> TISP_tableLookup_createNode(void            *pIn,
                                                             void            *pTable,
                                                             void            *pOut,
                                                             size_t           srcSamples,
                                                             size_t           tableSamples,
                                                             float            minVal,
                                                             float            maxVal,
                                                             uint32_t         tableLookupSize,
                                                             TISP::ErrorCtxt &errorCtx)
{
   const TISP::TISP_BufDesc2D inDesc  = {pIn, TISP::ElementDType<TIn>::value, srcSamples,
                                         1U,  srcSamples * sizeof(TIn),       0U};
   const TISP::TISP_BufDesc2D outDesc = {pOut, TISP::ElementDType<TOut>::value, srcSamples,
                                         1U,   srcSamples * sizeof(TOut),       0U};

   const TISP::TISP_BufDescList inputs  = {&inDesc, 1U};
   const TISP::TISP_BufDescList outputs = {&outDesc, 1U};

   TISP::AUDIOLIB::TablelookupConfig config;
   config.tableSamples    = tableSamples;
   config.minVal          = minVal;
   config.maxVal          = maxVal;
   config.tableLookupSize = tableLookupSize;

   TISP::AUDIOLIB::TablelookupTunableParams<TOut> defaultParams;
   defaultParams.pTable = static_cast<TOut *>(pTable);

   return std::make_unique<TISP::AUDIOLIB::Tablelookup<TIn, TOut>>(&inputs, &outputs, &config, &defaultParams,
                                                                   "Tablelookup", 0, errorCtx);
}

std::unique_ptr<TISP::VirtualOp> TISP_tableLookup_createKernelObject(void            *pIn,
                                                                     void            *pTable,
                                                                     void            *pOut,
                                                                     size_t           srcSamples,
                                                                     size_t           tableSamples,
                                                                     float            minVal,
                                                                     float            maxVal,
                                                                     uint32_t         tableLookupSize,
                                                                     uint32_t         sampleDataType,
                                                                     uint32_t         tableDataType,
                                                                     TISP::ErrorCtxt &errorCtx)
{
   if ((sampleDataType == AUDIOLIB_FLOAT32) && (tableDataType == AUDIOLIB_FLOAT32)) {
      return TISP_tableLookup_createNode<float, float>(pIn, pTable, pOut, srcSamples, tableSamples, minVal, maxVal,
                                                       tableLookupSize, errorCtx);
   }
   else if ((sampleDataType == AUDIOLIB_INT32) && (tableDataType == AUDIOLIB_FLOAT32)) {
      return TISP_tableLookup_createNode<int32_t, float>(pIn, pTable, pOut, srcSamples, tableSamples, minVal, maxVal,
                                                         tableLookupSize, errorCtx);
   }
   else if ((sampleDataType == AUDIOLIB_INT32) && (tableDataType == AUDIOLIB_INT32)) {
      return TISP_tableLookup_createNode<int32_t, int32_t>(pIn, pTable, pOut, srcSamples, tableSamples, minVal, maxVal,
                                                           tableLookupSize, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_tableLookup_createKernelObject",
                                        "unsupported data types for Tablelookup kernel");
      return nullptr;
   }
}

int TISP_tableLookup_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
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

   TISP_tableLookup_testParams_t *prm;
   TISP_tableLookup_testParams_t  currPrm;
   TISP_tableLookup_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_tableLookup");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_tableLookup.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "WidthIn, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      float  minVal          = currPrm.minVal;
      float  maxVal          = currPrm.maxVal;
      size_t tableLookupSize = currPrm.tableLookupSize;

      for (repCount = 0; repCount < numReps; repCount++) {
         int32_t status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;

         currentTestFail      = 0;
         size_t dataSize      = AUDIOLIB_sizeof(currPrm.sampleDataType);
         size_t tableDataSize = AUDIOLIB_sizeof(currPrm.tableDataType);
         size_t srcSamples    = currPrm.srcSamples;
         size_t tableSamples  = currPrm.tableSamples;
         size_t numBlocks     = currPrm.numBlocks;
         float  divisor       = (tableSamples - 1) / (maxVal - minVal + 1e-9);
         size_t inputSize     = numBlocks * srcSamples * dataSize;
         size_t tableSize     = tableSamples * tableDataSize;
         size_t outputSize    = numBlocks * srcSamples * tableDataSize;

         auto pIn          = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pTablelookup = myL2Heap.allocate64ByteAligned<uint8_t>(tableSize);
         auto pOut         = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);

         void *pOutCn = (void *) ddrBuffer;
         memset(pOutCn, 0, outputSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pTablelookup.get() && pOut.get() && pOutCn) {
            for (size_t n = 0; n < numBlocks; n++) {
               if (currPrm.sampleDataType == AUDIOLIB_FLOAT32 || currPrm.sampleDataType == AUDIOLIB_FLOAT64) {
                  // fill input buffer
                  TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn.get() + n * srcSamples * dataSize,
                                      prm[tpi].staticIn0 + n * srcSamples * dataSize, srcSamples, 1, 0, dataSize,
                                      testPatternString);
               }
               else {
                  // int32_t source: the samples are table indices; the table is float or int32_t
                  TI_fillBuffer(prm[tpi].testPattern, 0, pIn.get() + n * srcSamples * dataSize,
                                prm[tpi].staticIn0 + n * srcSamples * dataSize, srcSamples, 1, 0, dataSize,
                                testPatternString);
               }
            }
            TI_fillBuffer(prm[tpi].testPattern, 0, pTablelookup.get(), prm[tpi].staticIn1, tableSamples, 1, 0,
                          tableDataSize, testPatternString);

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_tableLookup_createKernelObject(
                           pIn.get(), pTablelookup.get(), pOut.get(), srcSamples, tableSamples, minVal, maxVal,
                           static_cast<uint32_t>(tableLookupSize), currPrm.sampleDataType, currPrm.tableDataType, errorCtx);

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
               kAddr.src[0] = (uint8_t *) pIn.get() + n * srcSamples * dataSize;
               kAddr.src[1] = (uint8_t *) pTablelookup.get();
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * srcSamples * tableDataSize;

               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }
#if !defined(__C7X_HOSTEM__)
            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */
            for (k = 0; k < NUM_REPS; k++) {

               for (size_t n = 0; n < numBlocks; n++) {

                  // set address for block to process
                  kAddrMode    = TISP::kAddrModeInOut;
                  kAddr.src[0] = (uint8_t *) pIn.get() + n * srcSamples * dataSize;
                  kAddr.src[1] = (uint8_t *) pTablelookup.get();
                  kAddr.dst[0] = (uint8_t *) pOut.get() + n * srcSamples * tableDataSize;

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
            for (k = 0; k < srcSamples; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            // dummy store of outSum to insure that the compiler does not remove it.
            volatileSum = outSum;

            for (size_t n = 0; n < numBlocks; n++) {

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * srcSamples * dataSize;
               kAddr.src[1] = (uint8_t *) pTablelookup.get();
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * srcSamples * tableDataSize;
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }
#endif
            inputSize = srcSamples;

            for (size_t n = 0; n < numBlocks; n++) {
               AUDIOLIB_tableLookup_cn(pIn.get() + n * srcSamples * dataSize, pTablelookup.get(),
                                       pOutCn + n * srcSamples * tableDataSize, currPrm.sampleDataType,
                                       currPrm.tableDataType, srcSamples, tableSamples, minVal, divisor);
            }

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            for (size_t n = 0; n < numBlocks; n++) {
               if (currPrm.tableDataType == AUDIOLIB_FLOAT32) {
                  status_nat_vs_opt &= TI_compare_mem_2D_float((void *) pOut.get() + n * srcSamples * tableDataSize,
                                                               (void *) pOutCn + n * srcSamples * tableDataSize, 0,
                                                               (double) powf(2, -10), srcSamples, 1, 0, tableDataSize);
               }
               else {
                  status_nat_vs_opt &= TI_compare_mem_2D((void *) pOut.get() + n * srcSamples * tableDataSize,
                                                         (void *) pOutCn + n * srcSamples * tableDataSize, 0, 0,
                                                         srcSamples, 1, 0, tableDataSize);
               }
            }

            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {

               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               for (size_t n = 0; n < numBlocks; n++) {
                  if (currPrm.tableDataType == AUDIOLIB_FLOAT32) {
                     status_ref_vs_opt &= TI_compare_mem_2DDualStride_float(
                         (void *) pOut.get() + n * srcSamples * tableDataSize,
                         (void *) currPrm.staticOut + n * srcSamples * tableDataSize, 0, (double) powf(2, -10),
                         srcSamples, 1, 0, srcSamples * tableDataSize, tableDataSize);
                  }
                  else {
                     status_ref_vs_opt &= TI_compare_mem_2D((void *) pOut.get() + n * srcSamples * tableDataSize,
                                                            (void *) currPrm.staticOut + n * srcSamples * tableDataSize,
                                                            0, 0, srcSamples, 1, 0, tableDataSize);
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

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString,
                    AUDIOLIB_sizeof(currPrm.sampleDataType));

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "TISP_tableLookup, %d, %d,  "
                    "%d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.srcSamples, archCycles, pProfile[3 * tpi + 1],
                    ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles), !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.srcSamples, 0, 0, currentTestFail, desc);
            TISP::handleAllocator().reset();
            TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", AUDIOLIB_sizeof(currPrm.sampleDataType),
                    currPrm.srcSamples, 1);
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
   return TISP_tableLookup_d(&pProfile[0], 0);
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
