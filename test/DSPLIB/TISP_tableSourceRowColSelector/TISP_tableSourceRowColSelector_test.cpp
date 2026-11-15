// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include DSPLIB
#include <dsplib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <tisp.hpp>

#include "TISP_node_factory.hpp"

// include test data for this kernel
#include "TISP_tableSourceRowColSelector_cn.h"
#include "TISP_tableSourceRowColSelector_idat.h"
#include "common/DSPLIB_bufParams.h"

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

template <typename T>
std::unique_ptr<TISP::VirtualOp> TISP_tableSourceRowColSelector_createNode(void            *pIn,
                                                                           void            *pTable,
                                                                           void            *pOut,
                                                                           void            *pScratch,
                                                                           size_t           scratchBytes,
                                                                           size_t           numIndices,
                                                                           size_t           numRows,
                                                                           size_t           numCols,
                                                                           uint32_t         isColSelect,
                                                                           uint32_t         isOutputInterleave,
                                                                           size_t           strideIn,
                                                                           size_t           strideOut,
                                                                           TISP::ErrorCtxt &errorCtx)
{
   const size_t selLen = (isColSelect == 0U) ? numCols : numRows;

   const TISP::TISP_BufDesc2D inDesc = {pIn, TISP::ElementDType<int32_t>::value, numIndices, 1U, strideIn, 0U};
   // One output row per index (non-interleaved) or one output column per index (interleaved)
   const TISP::TISP_BufDesc2D outDesc = {pOut,
                                         TISP::ElementDType<T>::value,
                                         (isOutputInterleave == 1U) ? numIndices : selLen,
                                         (isOutputInterleave == 1U) ? selLen : numIndices,
                                         strideOut,
                                         static_cast<uint8_t>(isOutputInterleave)};

   const TISP::TISP_BufDescList inputs  = {&inDesc, 1U};
   const TISP::TISP_BufDescList outputs = {&outDesc, 1U};

   TISP::DSPLIB::TableSourceRowColSelectorConfig config;
   config.numRows     = numRows;
   config.numCols     = numCols;
   config.isColSelect = static_cast<uint8_t>(isColSelect);

   TISP::DSPLIB::TableSourceRowColSelectorTunableParams<T> defaultParams;
   defaultParams.pTable = static_cast<T *>(pTable);

   return std::make_unique<TISP::DSPLIB::TableSourceRowColSelector<T>>(
       &inputs, &outputs, pScratch, scratchBytes, &config, &defaultParams, "TableSourceRowColSelector", 0, errorCtx);
}

std::unique_ptr<TISP::VirtualOp> TISP_tableSourceRowColSelector_createKernelObject(void            *pIn,
                                                                                   void            *pTable,
                                                                                   void            *pOut,
                                                                                   void            *pScratch,
                                                                                   size_t           scratchBytes,
                                                                                   size_t           numIndices,
                                                                                   size_t           numRows,
                                                                                   size_t           numCols,
                                                                                   uint32_t         isColSelect,
                                                                                   uint32_t         isOutputInterleave,
                                                                                   size_t           strideIn,
                                                                                   size_t           strideOut,
                                                                                   uint32_t         dataType,
                                                                                   TISP::ErrorCtxt &errorCtx)
{
   if (dataType == DSPLIB_INT32) {
      return TISP_tableSourceRowColSelector_createNode<int32_t>(pIn, pTable, pOut, pScratch, scratchBytes, numIndices,
                                                                numRows, numCols, isColSelect, isOutputInterleave,
                                                                strideIn, strideOut, errorCtx);
   }
   else if (dataType == DSPLIB_INT64) {
      return TISP_tableSourceRowColSelector_createNode<int64_t>(pIn, pTable, pOut, pScratch, scratchBytes, numIndices,
                                                                numRows, numCols, isColSelect, isOutputInterleave,
                                                                strideIn, strideOut, errorCtx);
   }
   else if (dataType == DSPLIB_UINT32) {
      return TISP_tableSourceRowColSelector_createNode<uint32_t>(pIn, pTable, pOut, pScratch, scratchBytes, numIndices,
                                                                 numRows, numCols, isColSelect, isOutputInterleave,
                                                                 strideIn, strideOut, errorCtx);
   }
   else if (dataType == DSPLIB_UINT64) {
      return TISP_tableSourceRowColSelector_createNode<uint64_t>(pIn, pTable, pOut, pScratch, scratchBytes, numIndices,
                                                                 numRows, numCols, isColSelect, isOutputInterleave,
                                                                 strideIn, strideOut, errorCtx);
   }
   else if (dataType == DSPLIB_FLOAT32) {
      return TISP_tableSourceRowColSelector_createNode<float>(pIn, pTable, pOut, pScratch, scratchBytes, numIndices,
                                                              numRows, numCols, isColSelect, isOutputInterleave,
                                                              strideIn, strideOut, errorCtx);
   }
   else if (dataType == DSPLIB_FLOAT64) {
      return TISP_tableSourceRowColSelector_createNode<double>(pIn, pTable, pOut, pScratch, scratchBytes, numIndices,
                                                               numRows, numCols, isColSelect, isOutputInterleave,
                                                               strideIn, strideOut, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_tableSourceRowColSelector_createKernelObject",
                                        "unsupported data type for TableSourceRowColSelector kernel");
      return nullptr;
   }
}

int TISP_tableSourceRowColSelector_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
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

   TISP_tableSourceRowColSelector_testParams_t *prm;
   TISP_tableSourceRowColSelector_testParams_t  currPrm;
   TISP_tableSourceRowColSelector_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_tableSourceRowColSelector");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_tableSourceRowColSelector.csv", "w");
   fprintf(fpOutputCSV,
           "Kernel, Test ID, Test type, "
           "NumIndices, NumRows, NumCols, IsColSelect, IsOutputInterleave, StrideIn, StrideOut, Arch cycles, EVM "
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

         currentTestFail             = 0;
         size_t   dataSize           = DSPLIB_sizeof(currPrm.dataType);
         size_t   strideIn           = currPrm.strideIn;
         size_t   strideOut          = currPrm.strideOut;
         size_t   numBlocks          = currPrm.numBlocks;
         size_t   numIndices         = currPrm.numIndices;
         size_t   numRows            = currPrm.numRows;
         size_t   numCols            = currPrm.numCols;
         uint32_t isColSelect        = currPrm.isColSelect;
         uint32_t isOutputInterleave = currPrm.isOutputInterleave;
         // Length of a selected table vector, and the output block: one row per index, or one column per index
         size_t selLen    = (isColSelect == 0U) ? numCols : numRows;
         size_t outWidth  = (isOutputInterleave == 1U) ? numIndices : selLen;
         size_t outHeight = (isOutputInterleave == 1U) ? selLen : numIndices;
         // one row of numIndices int32_t indices per block, strideIn bytes apart; outHeight rows of strideOut bytes
         // per block
         size_t inputSize         = numBlocks * strideIn;
         size_t tableSize         = numRows * numCols * dataSize;
         size_t outputSize        = numBlocks * outHeight * strideOut;
         size_t strideOutElements = strideOut / dataSize;
         // only the interleaved output uses the scratch buffer; its size depends only on the element size
         size_t scratchBytes =
             (isOutputInterleave == 0U) ? 0U
             : (dataSize == sizeof(float))
                 ? TISP::DSPLIB::TableSourceRowColSelector<float>::calcScratchSize(numIndices, selLen)
                 : TISP::DSPLIB::TableSourceRowColSelector<double>::calcScratchSize(numIndices, selLen);

         auto pIn      = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pTable   = myL2Heap.allocate64ByteAligned<uint8_t>(tableSize);
         auto pOut     = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);
         auto pScratch = myL2Heap.allocate64ByteAligned<uint8_t>(scratchBytes);

         void *pOutCn = (void *) ddrBuffer;
         memset(pOutCn, 0, outputSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pTable.get() && pOut.get() && pOutCn && ((scratchBytes == 0U) || pScratch.get())) {
            // fill index buffer: one row of numIndices indices per block, strideIn bytes apart
            TI_fillBuffer(prm[tpi].testPattern, 0, pIn.get(), prm[tpi].staticIn0, numIndices, numBlocks, strideIn,
                          sizeof(int32_t), testPatternString);
            // fill table buffer
            if (currPrm.dataType == DSPLIB_FLOAT32 || currPrm.dataType == DSPLIB_FLOAT64) {
               TI_fillBuffer_float(prm[tpi].testPattern, 0, pTable.get(), prm[tpi].staticIn1, numCols, numRows,
                                   numCols * dataSize, dataSize, testPatternString);
            }
            else {
               TI_fillBuffer(prm[tpi].testPattern, 0, pTable.get(), prm[tpi].staticIn1, numCols, numRows,
                             numCols * dataSize, dataSize, testPatternString);
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_tableSourceRowColSelector_createKernelObject(
                           pIn.get(), pTable.get(), pOut.get(), (scratchBytes == 0U) ? nullptr : pScratch.get(), scratchBytes,
                           numIndices, numRows, numCols, isColSelect, isOutputInterleave, strideIn, strideOut, currPrm.dataType,
                           errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               fclose(fpOutputCSV);
               return -1;
            }

            TI_profile_stop();

            // initialize set function parameters
            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;

            for (size_t n = 0; n < numBlocks; n++) {
               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * strideIn;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * outHeight * strideOut;

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
                  kAddr.src[0] = (uint8_t *) pIn.get() + n * strideIn;
                  kAddr.dst[0] = (uint8_t *) pOut.get() + n * outHeight * strideOut;

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
               kAddr.src[0] = (uint8_t *) pIn.get() + n * strideIn;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * outHeight * strideOut;
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            for (size_t n = 0; n < numBlocks; n++) {
               DSPLIB_tableSourceRowColSelector_cn(
                   (uint8_t *) pIn.get() + n * strideIn, pTable.get(), (uint8_t *) pOutCn + n * outHeight * strideOut,
                   currPrm.dataType, numIndices, numRows, numCols, isColSelect, isOutputInterleave, strideOutElements);
            }

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            for (size_t n = 0; n < numBlocks; n++) {
               if (currPrm.dataType == DSPLIB_FLOAT32 || currPrm.dataType == DSPLIB_FLOAT64) {
                  status_nat_vs_opt &=
                      TI_compare_mem_2D_float((void *) (pOut.get() + n * outHeight * strideOut),
                                              (void *) ((uint8_t *) pOutCn + n * outHeight * strideOut), 0.001,
                                              (double) powf(2, -10), outWidth, outHeight, strideOut, dataSize);
               }
               else {
                  status_nat_vs_opt &= TI_compare_mem_2D((void *) (pOut.get() + n * outHeight * strideOut),
                                                         (void *) ((uint8_t *) pOutCn + n * outHeight * strideOut), 0,
                                                         0, outWidth, outHeight, strideOut, dataSize);
               }
            }

            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               for (size_t n = 0; n < numBlocks; n++) {
                  // the reference output is packed: outHeight rows of outWidth elements per block
                  if (currPrm.dataType == DSPLIB_FLOAT32 || currPrm.dataType == DSPLIB_FLOAT64) {
                     status_ref_vs_opt &= TI_compare_mem_2DDualStride_float(
                         (void *) (pOut.get() + n * outHeight * strideOut),
                         (void *) ((uint8_t *) currPrm.staticOut + n * outHeight * outWidth * dataSize), 0.001,
                         (double) powf(2, -10), outWidth, outHeight, strideOut, outWidth * dataSize, dataSize);
                  }
                  else {
                     status_ref_vs_opt &= TI_compare_mem_2DDualStride(
                         (void *) (pOut.get() + n * outHeight * strideOut),
                         (void *) ((uint8_t *) currPrm.staticOut + n * outHeight * outWidth * dataSize), 0, 0, outWidth,
                         outHeight, strideOut, outWidth * dataSize, dataSize);
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

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString, DSPLIB_sizeof(currPrm.dataType));

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "TISP_tableSourceRowColSelector, %d, %d,  "
                    "%d, %d, %d, %d, %d, %d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.numIndices, currPrm.numRows, currPrm.numCols,
                    currPrm.isColSelect, currPrm.isOutputInterleave, currPrm.strideIn, currPrm.strideOut, archCycles,
                    pProfile[3 * tpi + 1], ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles),
                    !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.numIndices * selLen, 0, 0, currentTestFail, desc);
            TISP::handleAllocator().reset();
            TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, numIndices=%d, numRows=%d, numCols=%d, isColSelect=%d, isOutputInterleave=%d",
                    DSPLIB_sizeof(currPrm.dataType), currPrm.numIndices, currPrm.numRows, currPrm.numCols,
                    currPrm.isColSelect, currPrm.isOutputInterleave);
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

int test_main(uint32_t *pProfile) { return TISP_tableSourceRowColSelector_d(&pProfile[0], 0); }

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
