// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_mute_superNode_idat.h"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <audiolib.h>
#include <tisp.hpp>

#include "../../../src/superNode/TISP_dmaUtils.hpp"
#include "TISP_superNode.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <TISP_mute.hpp>
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

void TISP_mute_deleteSuperNode(TISP::SuperNode::superNodeVec *mySuperNodeVec)
{
   delete (mySuperNodeVec);
   return;
}

TISP::SuperNode::superNodeVec *TISP_mute_createSuperNode(void            *pIn,
                                                         void            *pOut,
                                                         void            *pInputBlock0,
                                                         void            *pOutputBlock,
                                                         uint32_t         widthIn,
                                                         uint32_t         heightIn,
                                                         uint32_t         inPitch,
                                                         uint32_t         outPitch,
                                                         uint32_t         blockWidth,
                                                         uint32_t         blockHeight,
                                                         uint8_t          isInterleaved,
                                                         int32_t          samplingRate,
                                                         uint32_t         isMute,
                                                         float            fadeTime,
                                                         uint32_t         fadeType,
                                                         uint32_t         dataType,
                                                         TISP::ErrorCtxt &errorCtx)
{
   if (dataType == AUDIOLIB_FLOAT32) {
      const TISP::TISP_BufDesc2D inDesc  = {pInputBlock0, TISP::ElementDType<float>::value, blockWidth, blockHeight,
                                            blockWidth * AUDIOLIB_sizeof(dataType), isInterleaved};
      const TISP::TISP_BufDesc2D outDesc = {pOutputBlock, TISP::ElementDType<float>::value, blockWidth, blockHeight,
                                            blockWidth * AUDIOLIB_sizeof(dataType), isInterleaved};

      const TISP::TISP_BufDescList inputs  = {&inDesc, 1U};
      const TISP::TISP_BufDescList outputs = {&outDesc, 1U};

      TISP::AUDIOLIB::MuteConfig config;
      config.samplingRate = samplingRate;

      TISP::AUDIOLIB::MuteTunableParams<float> defaultParams;
      defaultParams.isMute   = isMute;
      defaultParams.fadeTime = fadeTime;
      defaultParams.fadeType = fadeType;

      auto k0 = std::make_unique<TISP::AUDIOLIB::Mute<float>>(&inputs, &outputs, &config, &defaultParams, "Mute", 0,
                                                              errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating Mute<float> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec0 = std::make_unique<TISP::opVec>();

      myOpVec0->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2d<float, float>>(
          std::move(myOpVec0), (float *) pIn, (float *) pOut, (float *) pInputBlock0, (float *) pOutputBlock, widthIn,
          heightIn, blockWidth, blockHeight, widthIn * sizeof(float), widthIn * sizeof(float), TOTAL_DMA_CHANNELS);

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));
      return mySuperNodeVec;
   }
   else {
      errorCtx.setInvalidParameterError("TISP_mute_createSuperNode", "unsupported data type for SuperNode creation");
      return nullptr;
   }
}
int TISP_mute_superNode_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t tpi;
   int32_t currentTestFail;
   int32_t fail = 0;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;
   uint64_t estCycles  = 0;

   TISP_mute_superNode_testParams_t *prm;
   TISP_mute_superNode_testParams_t  currPrm;
   TISP_mute_superNode_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_mute_superNode");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_mute_superNode.csv", "w");
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

      size_t   widthIn, heightIn;
      size_t   blockWidth    = currPrm.blockWidth;
      size_t   blockHeight   = currPrm.blockHeight;
      size_t   inPitch       = currPrm.inPitch;
      size_t   outPitch      = currPrm.outPitch;
      size_t   isInterleaved = currPrm.isInterleaved;
      uint32_t isMute        = currPrm.isMute;
      float    fadeTime      = currPrm.fadeTime;
      uint32_t fadeType      = currPrm.fadeType;
      int32_t  samplingRate  = currPrm.samplingRate;
      size_t   datasize      = AUDIOLIB_sizeof(currPrm.dataType);

      if (isInterleaved) {
         widthIn  = currPrm.heightIn;
         heightIn = currPrm.widthIn;
      }
      else {
         widthIn  = currPrm.widthIn;
         heightIn = currPrm.heightIn;
      }
      size_t outputSize = heightIn * widthIn * datasize;

      /* 2x buffer allocation for ping-pong buffers */
      auto pInputBlock0 = myL2Heap.allocate64ByteAligned<uint8_t>(2 * blockHeight * blockWidth * datasize);
      auto pOutputBlock = myL2Heap.allocate64ByteAligned<uint8_t>(2 * blockHeight * blockWidth * datasize);

      void *pOut = (void *) ddrBuffer;
      memset(pOut, 0, outputSize);

      /* Only run the test if the buffer allocations fit in the heap */
      if (pInputBlock0.get() && pOutputBlock.get() && pOut) {

         TI_profile_start(TI_PROFILE_KERNEL_INIT);

         TISP::ErrorCtxt                errorCtx;
         TISP::SuperNode::superNodeVec *mySuperNodeVec =
             TISP_mute_createSuperNode(currPrm.staticIn0, pOut, pInputBlock0.get(), pOutputBlock.get(), widthIn,
                                       heightIn, inPitch, outPitch, blockWidth, blockHeight, isInterleaved,
                                       samplingRate, isMute, fadeTime, fadeType, currPrm.dataType, errorCtx);

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

            TISP::SuperNode::superNodeVec *mySuperNodeVec =
                TISP_mute_createSuperNode(currPrm.staticIn0, pOut, pInputBlock0.get(), pOutputBlock.get(), widthIn,
                                          heightIn, inPitch, outPitch, blockWidth, blockHeight, isInterleaved,
                                          samplingRate, isMute, fadeTime, fadeType, currPrm.dataType, errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating supernode: %s\n", errorCtx.getMessage());
               currentTestFail = 1;
               fail            = 1;
               continue;
            }

            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

            // Execute the SuperNode
            TISP::execute::graph(mySuperNodeVec);

            TISP_mute_deleteSuperNode(mySuperNodeVec);
            TISP::handleAllocator().reset();
            TISP::nodeAllocator().reset();

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

         for (uint32_t i = 0; i < 1; i++) {

            TISP::SuperNode::superNodeVec *mySuperNodeVec =
                TISP_mute_createSuperNode(currPrm.staticIn0, pOut, pInputBlock0.get(), pOutputBlock.get(), widthIn,
                                          heightIn, inPitch, outPitch, blockWidth, blockHeight, isInterleaved,
                                          samplingRate, isMute, fadeTime, fadeType, currPrm.dataType, errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating supernode: %s\n", errorCtx.getMessage());
               currentTestFail = 1;
               fail            = 1;
               continue;
            }

            // run warm instruction cache test
            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

            // Execute the SuperNode
            TISP::execute::graph(mySuperNodeVec);

            TI_profile_stop();

            TISP_mute_deleteSuperNode(mySuperNodeVec);
         }

         if (currPrm.staticOut != nullptr) {
            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               status_ref_vs_opt = TI_compare_mem_2DDualStride_float(
                   (void *) pOut, (void *) currPrm.staticOut, 0.001, (double) powf(2, -10), widthIn, heightIn, outPitch,
                   widthIn * datasize, AUDIOLIB_sizeof(currPrm.dataType));
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

         sprintf(desc, "%s generated input | Data size=%d ", testPatternString, AUDIOLIB_sizeof(currPrm.dataType));

         // write to CSV, must happen prior to write to screen because
         // TI_profile_formula_add clears values in counters
         uint64_t averageWarmCycles = pProfile[3 * tpi + 1];
         uint64_t dataSize          = heightIn * widthIn * AUDIOLIB_sizeof(currPrm.dataType);
         double   performanceGBPerSec =
             (double) 2 * dataSize /
             (double) averageWarmCycles; // 2*dataSize since we measure DDR/L2 bandwidth - I/P + O/P

         fprintf(fpOutputCSV,
                 "TISP_mute_superNode, %d, %ld,"
                 "%ld, %ld, %.2f, %.2f, %d\n",
                 testNum, dataSize, archCycles, averageWarmCycles, ((double) averageWarmCycles / (double) archCycles),
                 (performanceGBPerSec), !currentTestFail);

         TI_profile_add_test(testNum++, currPrm.widthIn * currPrm.heightIn, 0, 0, currentTestFail, desc);
         TISP_mute_deleteSuperNode(mySuperNodeVec);
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

   } // end idat test cases

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile) { return TISP_mute_superNode_d(&pProfile[0], 0); }

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
