// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_balance_superNode_idat.h"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <audiolib.h>
#include <cstddef>
#include <tisp.hpp>

#include "../../../src/superNode/TISP_dmaUtils.hpp"
#include "TISP_superNode.hpp"
#include "common/MATHLIB_types.h"
#include <TISP_balance.hpp>
#include <kernel/dpl/CacheP.h>

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) // 2MB KB
#endif

#define TOTAL_DMA_CHANNELS (4)

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

void TISP_balance_deleteSuperNode(TISP::SuperNode::superNodeVec *mySuperNodeVec)
{
   delete (mySuperNodeVec);
   return;
}

TISP::SuperNode::superNodeVec *TISP_balance_createSuperNode(void            *pInL,
                                                            void            *pInR,
                                                            void            *pOutL,
                                                            void            *pOutR,
                                                            void            *pInBlockLeft,
                                                            void            *pInBlockRight,
                                                            void            *pOutBlockLeft,
                                                            void            *pOutBlockRight,
                                                            int32_t          widthIn,
                                                            int32_t          heightIn,
                                                            int32_t          inPitch,
                                                            int32_t          outPitch,
                                                            float            balance,
                                                            int32_t          samplingRate,
                                                            float            smoothingTime,
                                                            int32_t          blockWidth,
                                                            int32_t          blockHeight,
                                                            uint32_t         dataType,
                                                            TISP::ErrorCtxt &errorCtx)
{
   if (dataType == AUDIOLIB_FLOAT32) {

      auto k0 = std::make_unique<TISP::AUDIOLIB::Balance<float>>(
          (float *) pInBlockLeft, (float *) pInBlockRight, (float *) pOutBlockLeft, (float *) pOutBlockRight,
          blockWidth, blockHeight, blockWidth * sizeof(float), blockWidth * sizeof(float), balance, smoothingTime,
          samplingRate, errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Balance<float> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));
      auto s0 = std::make_unique<TISP::SuperNode::I2dI2dToO2dO2d<float, float>>(
          std::move(myOpVec0), (float *) pInL, (float *) pInR, (float *) pOutL, (float *) pOutR, (float *) pInBlockLeft,
          (float *) pInBlockRight, (float *) pOutBlockLeft, (float *) pOutBlockRight, widthIn, heightIn, widthIn,
          heightIn, widthIn, heightIn, widthIn, heightIn, blockWidth, blockHeight, blockWidth, blockHeight, blockWidth,
          blockHeight, blockWidth, blockHeight, widthIn * sizeof(float), widthIn * sizeof(float), outPitch,
          widthIn * sizeof(float), TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else {
      errorCtx.setInvalidParameterError("TISP_balance_createSuperNode", "Unsupported data type for SuperNode creation");
      return nullptr;
   }
}

int TISP_balance_superNode_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t tpi;
   int32_t currentTestFail;
   int32_t fail = 0;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;
   uint64_t estCycles  = 0;

   TISP_balance_superNode_testParams_t *prm;
   TISP_balance_superNode_testParams_t  currPrm;
   TISP_balance_superNode_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_balance_superNode");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_balance_superNode.csv", "w+");
   fprintf(fpOutputCSV, "Kernel, Test ID, "
                        "Data size, Arch cycles, Warm Cycles, Warm/Arch cycles, Performance (GB/s), Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

#ifdef HOST_EMULATION
#if defined(_MSC_VER)
#else
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

      int32_t status_ref_vs_opt_left  = TI_TEST_KERNEL_FAIL;
      int32_t status_ref_vs_opt_right = TI_TEST_KERNEL_FAIL;

      currentTestFail = 0;

      float   balance       = currPrm.balance;
      int32_t samplingRate  = currPrm.samplingRate;
      float   smoothingTime = currPrm.smoothingTime;

      size_t dataSize    = AUDIOLIB_sizeof(currPrm.dataType);
      size_t widthIn     = currPrm.widthIn;
      size_t heightIn    = currPrm.heightIn;
      size_t strideIn    = currPrm.strideInElements;
      size_t strideOut   = currPrm.strideOutElements;
      size_t blockWidth  = currPrm.blockWidth;
      size_t blockHeight = currPrm.blockHeight;

      size_t outputSize = heightIn * strideIn;

      size_t allocPerBlock = 2 * blockWidth * blockHeight * dataSize;

      /* 2x buffer allocation for ping-pong buffers */
      auto pInBlockLeft   = myL2Heap.allocate64ByteAligned<uint8_t>(allocPerBlock);
      auto pOutBlockLeft  = myL2Heap.allocate64ByteAligned<uint8_t>(allocPerBlock);
      auto pInBlockRight  = myL2Heap.allocate64ByteAligned<uint8_t>(allocPerBlock);
      auto pOutBlockRight = myL2Heap.allocate64ByteAligned<uint8_t>(allocPerBlock);

      void *pOutLeft  = (void *) ddrBuffer;
      void *pOutRight = (void *) (ddrBuffer + outputSize);

      memset(pOutLeft, 0, outputSize);
      memset(pOutRight, 0, outputSize);

      /* Only run the test if the buffer allocations fit in the heap */
      if (pInBlockLeft.get() && pOutBlockLeft.get() && pInBlockRight.get() && pOutBlockRight && pOutLeft && pOutRight) {

         TI_profile_start(TI_PROFILE_KERNEL_INIT);

         TISP::ErrorCtxt                errorCtx;
         TISP::SuperNode::superNodeVec *mySuperNodeVec = TISP_balance_createSuperNode(
             currPrm.staticInLeft, currPrm.staticInRight, pOutLeft, pOutRight, pInBlockLeft.get(), pInBlockRight.get(),
             pOutBlockLeft.get(), pOutBlockRight.get(), widthIn, heightIn, strideIn, strideOut, balance, samplingRate,
             smoothingTime, blockWidth, blockHeight, currPrm.dataType, errorCtx);

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

            TISP::SuperNode::superNodeVec *mySuperNodeVecWarm = TISP_balance_createSuperNode(
                currPrm.staticInLeft, currPrm.staticInRight, pOutLeft, pOutRight, pInBlockLeft.get(),
                pInBlockRight.get(), pOutBlockLeft.get(), pOutBlockRight.get(), widthIn, heightIn, strideIn, strideOut,
                balance, samplingRate, smoothingTime, blockWidth, blockHeight, currPrm.dataType, errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating supernode: %s\n", errorCtx.getMessage());
               currentTestFail = 1;
               fail            = 1;
               continue;
            }

            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

            // Execute the SuperNode
            TISP::execute::graph(mySuperNodeVecWarm);

            TI_profile_stop();
            TISP_balance_deleteSuperNode(mySuperNodeVecWarm);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         //  get output to L1D
         int16_t outSum   = 0;
         int8_t *pOutTemp = (int8_t *) pOutLeft; // treat output as bytes to be data type agnostic
         for (k = 0; k < outputSize; k++) {
            outSum += *pOutTemp;
            pOutTemp++;
         }

         // dummy store of outSum to insurpOutTempe that the compiler does not remove it.
         volatileSum = outSum;

         for (uint32_t i = 0; i < 1; i++) {
            TISP::SuperNode::superNodeVec *mySuperNodeVecWrb = TISP_balance_createSuperNode(
                currPrm.staticInLeft, currPrm.staticInRight, pOutLeft, pOutRight, pInBlockLeft.get(),
                pInBlockRight.get(), pOutBlockLeft.get(), pOutBlockRight.get(), widthIn, heightIn, strideIn, strideOut,
                balance, samplingRate, smoothingTime, blockWidth, blockHeight, currPrm.dataType, errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating supernode: %s\n", errorCtx.getMessage());
               currentTestFail = 1;
               fail            = 1;
               continue;
            }

            // run warm instruction cache test
            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

            // Execute the SuperNode
            TISP::execute::graph(mySuperNodeVecWrb);

            TI_profile_stop();
            TISP_balance_deleteSuperNode(mySuperNodeVecWrb);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }

         if (currPrm.staticOutLeft != nullptr && currPrm.staticOutRight != nullptr) {
            status_ref_vs_opt_left = TI_compare_mem_2DDualStride_float(
                (void *) ((int8_t *) pOutLeft), (void *) ((int8_t *) currPrm.staticOutLeft), 0, (double) powf(2, -10),
                widthIn, heightIn, strideOut, widthIn * sizeof(float), dataSize);
            status_ref_vs_opt_right = TI_compare_mem_2DDualStride_float(
                (void *) ((int8_t *) pOutRight), (void *) ((int8_t *) currPrm.staticOutRight), 0, (double) powf(2, -10),
                widthIn, heightIn, strideOut, widthIn * sizeof(float), dataSize);
         }
         else {
            /* Set to pass since it wasn't supposed to run. */
            status_ref_vs_opt_left  = TI_TEST_KERNEL_PASS;
            status_ref_vs_opt_right = TI_TEST_KERNEL_PASS;
         }
         /* Set the 'fail' flag based on test vector comparison results */

         currentTestFail = ((status_ref_vs_opt_left == TI_TEST_KERNEL_FAIL) ||
                            (status_ref_vs_opt_right == TI_TEST_KERNEL_FAIL) || (currentTestFail == 1))
                               ? 1
                               : 0;

         fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

         pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[3 * tpi + 1] = (int32_t) (TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM) / NUM_REPS);
         pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

         sprintf(desc, "%s generated input | Data size=%ld ", testPatternString, dataSize);

         // write to CSV, must happen prior to write to screen because
         // TI_profile_formula_add clears values in counters
         uint64_t averageWarmCycles = pProfile[3 * tpi + 1];
         uint64_t datasize          = heightIn * widthIn * dataSize;
         double   performanceGBPerSec =
             (double) 2 * datasize /
             (double) averageWarmCycles; // 2*dataSize since we measure DDR/L2 bandwidth - I/P + O/P

         fprintf(fpOutputCSV,
                 "TISP_balance_superNode, %d, %ld,"
                 "%ld, %ld, %.2f, %.2f, %d\n",
                 testNum, datasize, archCycles, averageWarmCycles, ((double) averageWarmCycles / (double) archCycles),
                 performanceGBPerSec, !currentTestFail);

         TI_profile_add_test(testNum++, currPrm.widthIn * currPrm.heightIn, 0, 0, currentTestFail, desc);
         TISP_balance_deleteSuperNode(mySuperNodeVec);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
      }
      else {
         sprintf(desc, "Data size=%ld, widthIn size=%d, heightIn size=%d", dataSize, currPrm.widthIn, 1);
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

int test_main(uint32_t *pProfile) { return TISP_balance_superNode_d(&pProfile[0], 0); }

/* Main call for individual test projects */
#ifdef __cplusplus
extern "C" {
#endif
int test_main_d()
{
   int fail = 1;

   uint32_t profile[256 * 3];

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
