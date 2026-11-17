// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_tableSourceRowColSelector_superNode_idat.h"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <dsplib.h>
#include <tisp.hpp>

#include "../../../src/superNode/TISP_dmaUtils.hpp"
#include "TISP_superNode.hpp"
#include "common/DSPLIB_bufParams.h"
#include "drivers/sciclient/include/tisci/lpm/tisci_lpm.h"
#include <TISP_tableSourceRowColSelector.hpp>
#include <kernel/dpl/CacheP.h>

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) // 2MB KB
#endif

#define TOTAL_DMA_CHANNELS (3)

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
#endif // WIN32

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

void TISP_tableSourceRowColSelector_deleteSuperNode(TISP::SuperNode::superNodeVec *mySuperNodeVec)
{
   delete (mySuperNodeVec);
   return;
}

template <typename T>
std::unique_ptr<TISP::VirtualOp> TISP_tableSourceRowColSelector_createNode(void            *pInputBlock0,
                                                                           void            *pTable,
                                                                           void            *pOutputBlock,
                                                                           void            *pScratch,
                                                                           size_t           scratchBytes,
                                                                           size_t           blockWidth,
                                                                           size_t           numRows,
                                                                           size_t           numCols,
                                                                           uint32_t         isColSelect,
                                                                           uint32_t         isOutputInterleave,
                                                                           TISP::ErrorCtxt &errorCtx)
{
   const size_t selLen   = (isColSelect == 0U) ? numCols : numRows;
   const size_t outWidth = (isOutputInterleave == 1U) ? blockWidth : selLen;

   const TISP::TISP_BufDesc2D inDesc = {pInputBlock0, TISP::ElementDType<int32_t>::value, blockWidth,
                                        1U,           blockWidth * sizeof(int32_t),       0U};
   // One table row (non-interleaved) or column (interleaved) per index of the tile
   const TISP::TISP_BufDesc2D outDesc = {pOutputBlock,
                                         TISP::ElementDType<T>::value,
                                         outWidth,
                                         (isOutputInterleave == 1U) ? selLen : blockWidth,
                                         outWidth * sizeof(T),
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

TISP::SuperNode::superNodeVec *TISP_tableSourceRowColSelector_createSuperNode(void            *pIn,
                                                                              void            *pTable,
                                                                              void            *pOut,
                                                                              void            *pInputBlock0,
                                                                              void            *pOutputBlock,
                                                                              void            *pScratch,
                                                                              size_t           scratchBytes,
                                                                              size_t           inWidth,
                                                                              size_t           inHeight,
                                                                              size_t           strideIn,
                                                                              size_t           numRows,
                                                                              size_t           numCols,
                                                                              uint32_t         isColSelect,
                                                                              uint32_t         isOutputInterleave,
                                                                              size_t           outPitch,
                                                                              size_t           blockWidth,
                                                                              uint32_t         dataType,
                                                                              TISP::ErrorCtxt &errorCtx)
{
   // Each input tile is blockWidth indices of one input row; output tile k holds the vectors of input tile k, so the
   // frame output is one row per index (non-interleaved) or one column per index (interleaved) of the whole frame
   const size_t numIndices = inWidth * inHeight;
   const size_t selLen     = (isColSelect == 0U) ? numCols : numRows;
   const size_t outWidth   = (isOutputInterleave == 1U) ? numIndices : selLen;
   const size_t outHeight  = (isOutputInterleave == 1U) ? selLen : numIndices;
   const size_t outBlockW  = (isOutputInterleave == 1U) ? blockWidth : selLen;
   const size_t outBlockH  = (isOutputInterleave == 1U) ? selLen : blockWidth;

   if (dataType == DSPLIB_INT32) {
      auto k0 = TISP_tableSourceRowColSelector_createNode<int32_t>(pInputBlock0, pTable, pOutputBlock, pScratch,
                                                                   scratchBytes, blockWidth, numRows, numCols,
                                                                   isColSelect, isOutputInterleave, errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating TableSourceRowColSelector<int32_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dDiffDims<int32_t, int32_t>>(
          std::move(myOpVec0), (int32_t *) pIn, (int32_t *) pOut, (int32_t *) pInputBlock0, (int32_t *) pOutputBlock,
          inWidth, inHeight, outWidth, outHeight, blockWidth, 1, outBlockW, outBlockH, strideIn, outPitch,
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else if (dataType == DSPLIB_INT64) {
      auto k0 = TISP_tableSourceRowColSelector_createNode<int64_t>(pInputBlock0, pTable, pOutputBlock, pScratch,
                                                                   scratchBytes, blockWidth, numRows, numCols,
                                                                   isColSelect, isOutputInterleave, errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating TableSourceRowColSelector<int64_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dDiffDims<int32_t, int64_t>>(
          std::move(myOpVec0), (int32_t *) pIn, (int64_t *) pOut, (int32_t *) pInputBlock0, (int64_t *) pOutputBlock,
          inWidth, inHeight, outWidth, outHeight, blockWidth, 1, outBlockW, outBlockH, strideIn, outPitch,
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else if (dataType == DSPLIB_UINT32) {
      auto k0 = TISP_tableSourceRowColSelector_createNode<uint32_t>(pInputBlock0, pTable, pOutputBlock, pScratch,
                                                                    scratchBytes, blockWidth, numRows, numCols,
                                                                    isColSelect, isOutputInterleave, errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating TableSourceRowColSelector<uint32_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dDiffDims<int32_t, uint32_t>>(
          std::move(myOpVec0), (int32_t *) pIn, (uint32_t *) pOut, (int32_t *) pInputBlock0, (uint32_t *) pOutputBlock,
          inWidth, inHeight, outWidth, outHeight, blockWidth, 1, outBlockW, outBlockH, strideIn, outPitch,
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else if (dataType == DSPLIB_UINT64) {
      auto k0 = TISP_tableSourceRowColSelector_createNode<uint64_t>(pInputBlock0, pTable, pOutputBlock, pScratch,
                                                                    scratchBytes, blockWidth, numRows, numCols,
                                                                    isColSelect, isOutputInterleave, errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating TableSourceRowColSelector<uint64_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dDiffDims<int32_t, uint64_t>>(
          std::move(myOpVec0), (int32_t *) pIn, (uint64_t *) pOut, (int32_t *) pInputBlock0, (uint64_t *) pOutputBlock,
          inWidth, inHeight, outWidth, outHeight, blockWidth, 1, outBlockW, outBlockH, strideIn, outPitch,
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else if (dataType == DSPLIB_FLOAT32) {
      auto k0 = TISP_tableSourceRowColSelector_createNode<float>(pInputBlock0, pTable, pOutputBlock, pScratch,
                                                                 scratchBytes, blockWidth, numRows, numCols,
                                                                 isColSelect, isOutputInterleave, errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating TableSourceRowColSelector<float> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dDiffDims<int32_t, float>>(
          std::move(myOpVec0), (int32_t *) pIn, (float *) pOut, (int32_t *) pInputBlock0, (float *) pOutputBlock,
          inWidth, inHeight, outWidth, outHeight, blockWidth, 1, outBlockW, outBlockH, strideIn, outPitch,
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else if (dataType == DSPLIB_FLOAT64) {
      auto k0 = TISP_tableSourceRowColSelector_createNode<double>(pInputBlock0, pTable, pOutputBlock, pScratch,
                                                                  scratchBytes, blockWidth, numRows, numCols,
                                                                  isColSelect, isOutputInterleave, errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating TableSourceRowColSelector<double> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dDiffDims<int32_t, double>>(
          std::move(myOpVec0), (int32_t *) pIn, (double *) pOut, (int32_t *) pInputBlock0, (double *) pOutputBlock,
          inWidth, inHeight, outWidth, outHeight, blockWidth, 1, outBlockW, outBlockH, strideIn, outPitch,
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else {
      errorCtx.setInvalidParameterError("TISP_tableSourceRowColSelector_createSuperNode",
                                        "unsupported data type for SuperNode creation");
      return nullptr;
   }
}

int TISP_tableSourceRowColSelector_superNode_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t tpi;
   int32_t currentTestFail;
   int32_t fail = 0;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;
   uint64_t estCycles  = 0;

   TISP_tableSourceRowColSelector_superNode_testParams_t *prm;
   TISP_tableSourceRowColSelector_superNode_testParams_t  currPrm;
   TISP_tableSourceRowColSelector_superNode_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_tableSourceRowColSelector_superNode");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_tableSourceRowColSelector_superNode.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, "
                        "Data size, Arch cycles, Warm Cycles, Warm/Arch cycles, Performance (GB/s), Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

#ifdef HOST_EMULATION
#if defined(_MSC_VER)
   /* pIntMmeBase = (uint8_t *) _aligned_malloc(L2SRAM_SIZE, L2SRAM_SIZE); */
#else
   /* pIntMmeBase     = (uint8_t *) memalign(128, L2SRAM_SIZE); */
#endif
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

      currentTestFail = 0;

      size_t   inWidth            = currPrm.inWidth;
      size_t   inHeight           = currPrm.inHeight;
      size_t   strideIn           = currPrm.strideIn;
      size_t   numIndices         = inWidth * inHeight;
      size_t   numRows            = currPrm.numRows;
      size_t   numCols            = currPrm.numCols;
      uint32_t isColSelect        = currPrm.isColSelect;
      uint32_t isOutputInterleave = currPrm.isOutputInterleave;
      size_t   blockWidth         = currPrm.blockWidth;
      size_t   outPitch           = currPrm.outPitch;
      size_t   datasize           = DSPLIB_sizeof(currPrm.dataType);
      // one row (non-interleaved) or one column (interleaved) of selLen values per index, rows outPitch bytes apart
      size_t selLen     = (isColSelect == 0U) ? numCols : numRows;
      size_t outWidth   = (isOutputInterleave == 1U) ? numIndices : selLen;
      size_t outHeight  = (isOutputInterleave == 1U) ? selLen : numIndices;
      size_t outputSize = outHeight * outPitch;
      // only the interleaved output uses the scratch buffer, sized for one tile; its size depends only on the
      // element size
      size_t scratchBytes = (isOutputInterleave == 0U) ? 0U
                            : (datasize == sizeof(float))
                                ? TISP::DSPLIB::TableSourceRowColSelector<float>::calcScratchSize(blockWidth, selLen)
                                : TISP::DSPLIB::TableSourceRowColSelector<double>::calcScratchSize(blockWidth, selLen);

      /* 2x buffer allocation for ping-pong buffers */
      auto pInputBlock0 = myL2Heap.allocate64ByteAligned<uint8_t>(2 * blockWidth * sizeof(int32_t));
      auto pOutputBlock = myL2Heap.allocate64ByteAligned<uint8_t>(2 * blockWidth * selLen * datasize);
      auto pTable       = myL2Heap.allocate64ByteAligned<uint8_t>(numRows * numCols * datasize);
      auto pScratch     = myL2Heap.allocate64ByteAligned<uint8_t>(scratchBytes);

      void *pOut = (void *) ddrBuffer;

      /* Only run the test if the buffer allocations fit in the heap and the DDR buffer */
      if (pInputBlock0.get() && pOutputBlock.get() && pTable.get() && ((scratchBytes == 0U) || pScratch.get()) &&
          (outputSize <= sizeof(ddrBuffer))) {

         memset(pOut, 0, outputSize);

         // the table stays in L2 for the whole frame
         if (currPrm.dataType == DSPLIB_FLOAT32 || currPrm.dataType == DSPLIB_FLOAT64) {
            TI_fillBuffer_float(prm[tpi].testPattern, 0, pTable.get(), prm[tpi].staticIn1, numCols, numRows,
                                numCols * datasize, datasize, testPatternString);
         }
         else {
            TI_fillBuffer(prm[tpi].testPattern, 0, pTable.get(), prm[tpi].staticIn1, numCols, numRows,
                          numCols * datasize, datasize, testPatternString);
         }

         TI_profile_start(TI_PROFILE_KERNEL_INIT);

         // the frame input is the static input: inHeight rows of inWidth indices, strideIn bytes apart
         TISP::ErrorCtxt                errorCtx;
         TISP::SuperNode::superNodeVec *mySuperNodeVec = TISP_tableSourceRowColSelector_createSuperNode(
             currPrm.staticIn0, pTable.get(), pOut, pInputBlock0.get(), pOutputBlock.get(),
             (scratchBytes == 0U) ? nullptr : pScratch.get(), scratchBytes, inWidth, inHeight, strideIn, numRows,
             numCols, isColSelect, isOutputInterleave, outPitch, blockWidth, currPrm.dataType, errorCtx);

         if (!errorCtx.isSuccess()) {
            printf("\nError creating supernode: %s\n", errorCtx.getMessage());
            currentTestFail = 1;
            fail            = 1;
            continue;
         }

         TI_profile_stop();

         TI_profile_start(TI_PROFILE_KERNEL_OPT);

         // Execute the SuperNode
         TISP::execute::graph(mySuperNodeVec);

         TI_profile_stop();

         /* The following for loop is to call kernel repeatedly so as to
          * train the branch predictor                                   */
         for (k = 0; k < NUM_REPS; k++) {

            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

            // Execute the SuperNode
            TISP::execute::graph(mySuperNodeVec);

            TI_profile_stop();
         }

         // get output to L1D
         int16_t outSum   = 0;
         int8_t *pOutTemp = (int8_t *) pOut; // treat output as bytes to be data type agnostic
         for (k = 0; k < outputSize; k++) {
            outSum += *pOutTemp;
            pOutTemp++;
         }

         // dummy store of outSum to insure that the compiler does not remove it.
         volatileSum = outSum;

         // run warm instruction cache test
         TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

         // Execute the SuperNode
         TISP::execute::graph(mySuperNodeVec);

         TI_profile_stop();

         if (currPrm.staticOut != nullptr) {
            if (currPrm.dataType == DSPLIB_FLOAT32 || currPrm.dataType == DSPLIB_FLOAT64) {
               status_ref_vs_opt = TI_compare_mem_2DDualStride_float((void *) pOut, (void *) currPrm.staticOut, 0.001,
                                                                     (double) powf(2, -10), outWidth, outHeight,
                                                                     outPitch, outWidth * datasize, datasize);
            }
            else {
               status_ref_vs_opt =
                   TI_compare_mem_2DDualStride((void *) pOut, (void *) currPrm.staticOut, 0, 0, outWidth, outHeight,
                                               outPitch, outWidth * datasize, datasize);
            }
         }
         else {
            /* Set to pass since it wasn't supposed to run. */
            status_ref_vs_opt = TI_TEST_KERNEL_PASS;
         }
         /* Set the 'fail' flag based on test vector comparison results */

         currentTestFail = ((status_ref_vs_opt == TI_TEST_KERNEL_FAIL) || (currentTestFail == 1)) ? 1 : 0;

         fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

         pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[3 * tpi + 1] = (int32_t) (TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM) / NUM_REPS);
         pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

         sprintf(desc, "%s generated input | Data size=%d ", testPatternString, DSPLIB_sizeof(currPrm.dataType));

         // write to CSV, must happen prior to write to screen because
         // TI_profile_formula_add clears values in counters
         uint64_t averageWarmCycles = pProfile[3 * tpi + 1];
         uint64_t dataSize          = numIndices * selLen * DSPLIB_sizeof(currPrm.dataType);
         double   performanceGBPerSec =
             (double) 2 * dataSize /
             (double) averageWarmCycles; // 2*dataSize since we measure DDR/L2 bandwidth - I/P + O/P

         fprintf(fpOutputCSV,
                 "TISP_tableSourceRowColSelector_superNode, %d, %ld,"
                 "%ld, %ld, %.2f, %.2f, %d\n",
                 testNum, dataSize, archCycles, averageWarmCycles, ((double) averageWarmCycles / (double) archCycles),
                 performanceGBPerSec, !currentTestFail);

         TI_profile_add_test(testNum++, numIndices * selLen, 0, 0, currentTestFail, desc);
         TISP_tableSourceRowColSelector_deleteSuperNode(mySuperNodeVec);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
      }
      else {
         sprintf(desc,
                 "Data size=%d, inWidth=%d, inHeight=%d, numRows=%d, numCols=%d, isColSelect=%d, isOutputInterleave=%d",
                 DSPLIB_sizeof(currPrm.dataType), currPrm.inWidth, currPrm.inHeight, currPrm.numRows, currPrm.numCols,
                 currPrm.isColSelect, currPrm.isOutputInterleave);
         TI_profile_skip_test(desc);
         // clear the counters between runs; normally handled by
         // TI_profile_add_test
         TI_profile_clear_run_stats();
      } // end of memory allocation successful?

   } // end idat test cases

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile) { return TISP_tableSourceRowColSelector_superNode_d(&pProfile[0], 0); }

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
