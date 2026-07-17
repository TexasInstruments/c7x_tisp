// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <dsplib.h>
#include <tisp.hpp>
// include test data for this kernel
#include "../../../src/superNode/TISP_dmaUtils.hpp"
#include "TISP_deinterleave_superNode_idat.h"
#include <kernel/dpl/CacheP.h>

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) // 2MB KB
#endif

#define TOTAL_DMA_CHANNELS (2)

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

void TISP_deinterleave_deleteSuperNode(TISP::SuperNode::superNodeVec *mySuperNodeVec)
{
   delete (mySuperNodeVec);
   return;
}

TISP::SuperNode::superNodeVec *TISP_deinterleave_createSuperNode(void            *pIn,
                                                                 void            *pOut,
                                                                 void            *pInputBlock,
                                                                 void            *pOutputBlock,
                                                                 int32_t          widthIn,
                                                                 int32_t          heightIn,
                                                                 int32_t          inPitch,
                                                                 int32_t          outPitch,
                                                                 int32_t          blockWidth,
                                                                 int32_t          blockHeight,
                                                                 uint32_t         dataType,
                                                                 TISP::ErrorCtxt &errorCtx)
{

   if (dataType == DSPLIB_FLOAT32) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<float>>(
          (float *) pInputBlock, (float *) pOutputBlock, blockWidth, blockHeight, blockWidth * DSPLIB_sizeof(dataType),
          blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<float> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<float, float>>(
          std::move(myOpVec0), (float *) pIn, (float *) pOut, (float *) pInputBlock, (float *) pOutputBlock, widthIn,
          heightIn, blockWidth, blockHeight, widthIn * sizeof(float), heightIn * sizeof(float), TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else if (dataType == DSPLIB_FLOAT64) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<double>>(
          (double *) pInputBlock, (double *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<double> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<double, double>>(
          std::move(myOpVec0), (double *) pIn, (double *) pOut, (double *) pInputBlock, (double *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(double), heightIn * sizeof(double),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else if (dataType == DSPLIB_INT8) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<int8_t>>(
          (int8_t *) pInputBlock, (int8_t *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<int8_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<int8_t, int8_t>>(
          std::move(myOpVec0), (int8_t *) pIn, (int8_t *) pOut, (int8_t *) pInputBlock, (int8_t *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(int8_t), heightIn * sizeof(int8_t),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }

   else if (dataType == DSPLIB_UINT8) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<uint8_t>>(
          (uint8_t *) pInputBlock, (uint8_t *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<uint8_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<uint8_t, uint8_t>>(
          std::move(myOpVec0), (uint8_t *) pIn, (uint8_t *) pOut, (uint8_t *) pInputBlock, (uint8_t *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(uint8_t), heightIn * sizeof(uint8_t),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }

   else if (dataType == DSPLIB_INT16) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<int16_t>>(
          (int16_t *) pInputBlock, (int16_t *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<int16_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<int16_t, int16_t>>(
          std::move(myOpVec0), (int16_t *) pIn, (int16_t *) pOut, (int16_t *) pInputBlock, (int16_t *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(int16_t), heightIn * sizeof(int16_t),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }

   else if (dataType == DSPLIB_UINT16) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<uint16_t>>(
          (uint16_t *) pInputBlock, (uint16_t *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<uint16_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<uint16_t, uint16_t>>(
          std::move(myOpVec0), (uint16_t *) pIn, (uint16_t *) pOut, (uint16_t *) pInputBlock, (uint16_t *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(uint16_t), heightIn * sizeof(uint16_t),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }

   else if (dataType == DSPLIB_INT32) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<int32_t>>(
          (int32_t *) pInputBlock, (int32_t *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<int32_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<int32_t, int32_t>>(
          std::move(myOpVec0), (int32_t *) pIn, (int32_t *) pOut, (int32_t *) pInputBlock, (int32_t *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(int32_t), heightIn * sizeof(int32_t),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }

   else if (dataType == DSPLIB_UINT32) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<uint32_t>>(
          (uint32_t *) pInputBlock, (uint32_t *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<uint32_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<uint32_t, uint32_t>>(
          std::move(myOpVec0), (uint32_t *) pIn, (uint32_t *) pOut, (uint32_t *) pInputBlock, (uint32_t *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(uint32_t), heightIn * sizeof(uint32_t),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }

   else if (dataType == DSPLIB_INT64) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<int64_t>>(
          (int64_t *) pInputBlock, (int64_t *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<int64_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<int64_t, int64_t>>(
          std::move(myOpVec0), (int64_t *) pIn, (int64_t *) pOut, (int64_t *) pInputBlock, (int64_t *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(int64_t), heightIn * sizeof(int64_t),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }

   else if (dataType == DSPLIB_UINT64) {

      auto k0 = std::make_unique<TISP::DSPLIB::Deinterleave<uint64_t>>(
          (uint64_t *) pInputBlock, (uint64_t *) pOutputBlock, blockWidth, blockHeight,
          blockWidth * DSPLIB_sizeof(dataType), blockHeight * DSPLIB_sizeof(dataType), errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Deinterleave<uint64_t> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<uint64_t, uint64_t>>(
          std::move(myOpVec0), (uint64_t *) pIn, (uint64_t *) pOut, (uint64_t *) pInputBlock, (uint64_t *) pOutputBlock,
          widthIn, heightIn, blockWidth, blockHeight, widthIn * sizeof(uint64_t), heightIn * sizeof(uint64_t),
          TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }

   else {
      errorCtx.setInvalidParameterError("TISP_deinterleave_createSuperNode",
                                        "Unsupported data type for SuperNode creation");
      return nullptr;
   }
}

int TISP_deinterleave_superNode_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t tpi;
   int32_t currentTestFail;
   int32_t fail = 0;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;

   TISP_deinterleave_superNode_testParams_t *prm;
   TISP_deinterleave_superNode_testParams_t  currPrm;
   TISP_deinterleave_superNode_getTestParams(&prm, &test_cases);

   TI_profile_init((char *) "TISP_deinterleave_superNode");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_deinterleave_superNode.csv", "w");
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

      size_t widthIn     = currPrm.widthIn;
      size_t heightIn    = currPrm.heightIn;
      size_t blockWidth  = currPrm.blockWidth;
      size_t blockHeight = currPrm.blockHeight;
      size_t inPitch     = currPrm.inPitch;
      size_t outPitch    = currPrm.outPitch;
      size_t datasize    = DSPLIB_sizeof(currPrm.dataType);
      size_t outputSize  = heightIn * widthIn * datasize;

      /* 2x buffer allocation for ping-pong buffers */
      auto pInputBlock  = myL2Heap.allocate64ByteAligned<uint8_t>(2 * blockHeight * blockWidth * datasize);
      auto pOutputBlock = myL2Heap.allocate64ByteAligned<uint8_t>(2 * blockHeight * blockWidth * datasize);

      void *pOut = (void *) ddrBuffer;
      memset(pOut, 0, outputSize);

      /* Only run the test if the buffer allocations fit in the heap */
      if (pInputBlock.get() && pOutputBlock.get() && pOut) {

         TI_profile_start(TI_PROFILE_KERNEL_INIT);

         TISP::ErrorCtxt                errorCtx;
         TISP::SuperNode::superNodeVec *mySuperNodeVec = TISP_deinterleave_createSuperNode(
             currPrm.staticIn, pOut, pInputBlock.get(), pOutputBlock.get(), widthIn, heightIn, inPitch, outPitch,
             blockWidth, blockHeight, currPrm.dataType, errorCtx);

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

            // TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

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

         // dummy store of outSum to insurpOutTempe that the compiler does not remove it.
         volatileSum = outSum;

         // run warm instruction cache test
         TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

         // Execute the SuperNode
         TISP::execute::graph(mySuperNodeVec);

         TI_profile_stop();

         if (currPrm.staticOut != nullptr) {
            if (currPrm.dataType == DSPLIB_FLOAT32 || currPrm.dataType == DSPLIB_FLOAT64) {

               status_ref_vs_opt = TI_compare_mem_roi2DStatic_float((void *) pOut, (void *) currPrm.staticOut, 0.001,
                                                                    (double) powf(2, -10), heightIn, widthIn, outPitch,
                                                                    DSPLIB_sizeof(currPrm.dataType));
            }
            else {
               status_ref_vs_opt = TI_compare_mem_roi2DStatic((void *) pOut, (void *) currPrm.staticOut, 0, 0, heightIn,
                                                              widthIn, outPitch, DSPLIB_sizeof(currPrm.dataType));
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

         sprintf(desc, (char *) "%s generated input | Data size=%d ", testPatternString,
                 DSPLIB_sizeof(currPrm.dataType));

         // write to CSV, must happen prior to write to screen because
         // TI_profile_formula_add clears values in counters
         uint64_t averageWarmCycles = pProfile[3 * tpi + 1];
         uint64_t dataSize          = heightIn * widthIn * DSPLIB_sizeof(currPrm.dataType);

         fprintf(fpOutputCSV,
                 "TISP_deinterleave_superNode, %d, %ld,"
                 "%ld, %ld, %.2f, %.2f, %d\n",
                 testNum, dataSize, archCycles, averageWarmCycles, ((double) averageWarmCycles / (double) archCycles),
                 (2 * (double) dataSize / (double) averageWarmCycles), !currentTestFail);

         TI_profile_add_test(testNum++, currPrm.widthIn * currPrm.heightIn, 0, 0, currentTestFail, desc);
         TISP_deinterleave_deleteSuperNode(mySuperNodeVec);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
      }
      else {
         sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", DSPLIB_sizeof(currPrm.dataType),
                 currPrm.widthIn, currPrm.heightIn);
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

int test_main(uint32_t *pProfile) { return TISP_deinterleave_superNode_d(&pProfile[0], 0); }

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
