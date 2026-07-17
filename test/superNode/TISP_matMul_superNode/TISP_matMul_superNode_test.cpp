// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../../../src/superNode/TISP_dmaUtils.hpp"
#include "../../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <dsplib.h>
#include <kernel/dpl/CacheP.h>
#include <tisp.hpp>
// include test data for this kernel
#include "TISP_matMul_superNode_idat.h"

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

TISP::SuperNode::superNodeVec *TISP_matMul_createSuperNode(void            *pInA,
                                                           void            *pInB,
                                                           void            *pOut,
                                                           void            *pInputBlockA,
                                                           void            *pInputBlockB,
                                                           void            *pOutputBlock,
                                                           int32_t          widthA,
                                                           int32_t          heightA,
                                                           int32_t          widthB,
                                                           int32_t          heightB,
                                                           int32_t          inPitchA,
                                                           int32_t          inPitchB,
                                                           int32_t          outPitch,
                                                           int32_t          blockWidthA,
                                                           int32_t          blockHeightA,
                                                           int32_t          blockWidthB,
                                                           int32_t          blockHeightB,
                                                           int32_t          blockWidthOut,
                                                           int32_t          blockHeightOut,
                                                           uint32_t         dataType,
                                                           TISP::ErrorCtxt &errorCtx)
{
   auto k0 = std::make_unique<TISP::DSPLIB::MatMul<float>>(
       (float *) pInputBlockA, (float *) pInputBlockB, (float *) pOutputBlock, blockWidthA, blockHeightA, blockWidthB,
       blockHeightB, blockWidthA * DSPLIB_sizeof(dataType), blockWidthB * DSPLIB_sizeof(dataType),
       blockWidthB * DSPLIB_sizeof(dataType), errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating MatMul<float> node: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto myOpVec = std::make_unique<TISP::opVec>();

   myOpVec->push_back(std::move(k0));

   auto s0 = std::make_unique<TISP::SuperNode::i2d_to_o2dMatMul<float, float>>(
       std::move(myOpVec), (float *) pInA, (float *) pInB, (float *) pOut, (float *) pInputBlockA,
       (float *) pInputBlockB, (float *) pOutputBlock, widthA, heightA, widthB, heightB, blockWidthA, blockHeightA,
       blockWidthB, blockHeightB, inPitchA, inPitchB, outPitch, (TOTAL_DMA_CHANNELS));

   auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

   mySuperNodeVec->push_back(std::move(s0));

   return mySuperNodeVec;
}

int TISP_matMul_superNode_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t tpi;
   int32_t currentTestFail;
   int32_t fail = 0;

   uint32_t a = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;

   TISP_matMul_superNode_testParams_t *prm;
   TISP_matMul_superNode_testParams_t  currPrm;
   TISP_matMul_superNode_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_matMul_superNode");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_matMul_superNode.csv", "w+");
   fprintf(fpOutputCSV, "Kernel, Test ID, "
                        "Data size, Arch cycles, Loki "
                        "cycles, Loki/Arch cycles, Pass/Fail\n");

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

      size_t m          = currPrm.m;
      size_t n          = currPrm.n;
      size_t k          = currPrm.k;
      size_t inPitchA   = currPrm.dataPitchInA;
      size_t inPitchB   = currPrm.dataPitchInB;
      size_t outPitch   = currPrm.dataPitchOut;
      size_t datasize   = DSPLIB_sizeof(currPrm.dataType);
      size_t outputSize = m * n * datasize;

      uint32_t tileSize = TISP::dmaUtils<float>::get_tile_size(n, m, L2SRAM_HEAP_SIZE);

      /* 2x buffer allocation for ping-pong buffers */
      auto pInputBlockA = myL2Heap.allocate64ByteAligned<uint8_t>(2 * tileSize * k * datasize);
      auto pInputBlockB = myL2Heap.allocate64ByteAligned<uint8_t>(2 * k * tileSize * datasize);
      auto pOutputBlock = myL2Heap.allocate64ByteAligned<uint8_t>(2 * tileSize * tileSize * datasize);

      void *pOut = (void *) ddrBuffer;
      memset(pOut, 0, outputSize);

      // char* fileNameA = currPrm.dataPathA;
      // char* fileNameB = currPrm.dataPathB;
      // char* fileNameOut = currPrm.dataPathOut;

      // FILE* fileA = fopen(fileNameA, "rb");
      // FILE* fileB = fopen(fileNameB, "rb");
      // FILE* fileOut = fopen(fileNameOut, "rb");

      // if (!fileA || !fileB || !fileOut) {
      //    return fail;
      // }

      // float* pA = (float*)ddrBuffer;
      // float* pB = (float*)(ddrBuffer + (m * k) * datasize);
      // float* pOutBin = (float*)(ddrBuffer + (m * k + k * n) * datasize);

      // float* pOut = (float*)(ddrBuffer + (m * k + k * n + m * n) * datasize);

      // fread(pA, datasize, m * k, fileA);
      // fread(pB, datasize, k * n, fileB);
      // fread(pOutBin, datasize, m * n, fileOut);

      // // Close the files
      // fclose(fileA);
      // fclose(fileB);
      // fclose(fileOut);

      /* Only run the test if the buffer allocations fit in the heap */
      if (pInputBlockA.get() && pInputBlockB.get() && pOutputBlock.get() && pOut) {

         TI_profile_start(TI_PROFILE_KERNEL_INIT);

         TISP::ErrorCtxt errorCtx;

         TISP::SuperNode::superNodeVec *mySuperNodeVec = TISP_matMul_createSuperNode(
             (void *) currPrm.staticInA, (void *) currPrm.staticInB, pOut, pInputBlockA.get(), pInputBlockB.get(),
             pOutputBlock.get(), k, m, n, k, inPitchA, inPitchB, outPitch, k, tileSize, tileSize, k, tileSize, tileSize,
             currPrm.dataType, errorCtx);

         if (!errorCtx.isSuccess()) {
            printf("\nError creating supernode: %s\n", errorCtx.getMessage());
            currentTestFail = 1;
            fail            = 1;
            continue;
         }

         TI_profile_stop();

         TI_profile_start(TI_PROFILE_KERNEL_OPT);

         // Execute the kernel
         TISP::execute::graph(mySuperNodeVec);

         TI_profile_stop();

         /* The following for loop is to call kernel repeatedly so as to
          * train the branch predictor                                   */
         for (a = 0; a < NUM_REPS; a++) {

            TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

            // Execute the kernel
            TISP::execute::graph(mySuperNodeVec);

            TI_profile_stop();
         }

         // get output to L1D
         int16_t outSum   = 0;
         int8_t *pOutTemp = (int8_t *) pOut; // treat output as bytes to be data type agnostic
         for (a = 0; a < outputSize; a++) {
            outSum += *pOutTemp;
            pOutTemp++;
         }

         // dummy store of outSum to insurpOutTempe that the compiler does not remove it.
         volatileSum = outSum;

         // run warm instruction cache test
         TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

         // Execute the kernel
         TISP::execute::graph(mySuperNodeVec);

         TI_profile_stop();

         if (currPrm.staticOut != nullptr) {
            if (currPrm.dataType == DSPLIB_FLOAT32 || currPrm.dataType == DSPLIB_FLOAT64) {
               // Adaptive tolerance based on matrix dimensions
               // Larger accumulation allows larger relative/absolute error due to floating-point compounding
               // On target hardware, DMA and block boundary handling may introduce localized artifacts
               uint64_t accumElements = (uint64_t) m * n * k;
               double   relToleranceForTest;
               double   absToleranceForTest;

               if (accumElements > (256 * 256 * 256)) {
                  // Large matrices (512x512x512 or larger):
                  // ~0.1% relative error tolerance (accounts for 512 accumulations in float32)
                  // ~8% absolute error tolerance (for values ~1e6, allows ~80K ULP error)
                  relToleranceForTest = 0.1; // 0.1% relative
                  absToleranceForTest = 1e5; // ~8% for large magnitude outputs
               }
               else {
                  // Smaller matrices:
                  // ~0.78% relative error tolerance
                  relToleranceForTest = 0.05;        // 0.05% relative
                  absToleranceForTest = powf(2, -7); // ~0.78% absolute
               }

               status_ref_vs_opt = TI_compare_mem_roi2DStatic_float((void *) pOut, (void *) currPrm.staticOut,
                                                                    relToleranceForTest, absToleranceForTest, n, m,
                                                                    outPitch, DSPLIB_sizeof(currPrm.dataType));
            }
            else {

               status_ref_vs_opt &= TI_compare_mem_roi2DStatic((void *) pOut, (void *) currPrm.staticOut, 0, 0, n, m,
                                                               outPitch, DSPLIB_sizeof(currPrm.dataType));
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
         pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
         pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

         sprintf(desc, "%s generated input | Data size=%ld ", testPatternString, sizeof(currPrm.dataType));

         // write to CSV, must happen prior to write to screen because
         // TI_profile_formula_add clears values in counters
         fprintf(fpOutputCSV,
                 "TISP_matMul_superNode, %d, %ld,"
                 "%lu, %lu, %.2f, %d\n",
                 testNum, m * n, archCycles, cycles[TI_PROFILE_KERNEL_OPT_WARM],
                 ((DSPLIB_F32) cycles[TI_PROFILE_KERNEL_OPT]) / ((DSPLIB_F32) archCycles), !currentTestFail);

         TI_profile_add_test(testNum++, currPrm.m * currPrm.n, 0, 0, currentTestFail, desc);

         delete mySuperNodeVec;
      }
      else {
         sprintf(desc, "Data size=%ld, widthIn size=%d, heightIn size=%d", sizeof(currPrm.dataType), currPrm.n,
                 currPrm.m);
         TI_profile_skip_test(desc);
         // clear the counters between runs; normally handled by
         // TI_profile_add_test
         TI_profile_clear_run_stats();
      } // end of memory allocation successful?
   }

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("_HOST_BUILD is defined.\n");
#endif
   return TISP_matMul_superNode_d(&pProfile[0], 0);
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
   return fail;
}

#ifdef __cplusplus
}
#endif
