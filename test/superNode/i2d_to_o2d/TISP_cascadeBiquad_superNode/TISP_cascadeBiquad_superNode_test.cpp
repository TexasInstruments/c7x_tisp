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
#include "TISP_cascadeBiquad_superNode_idat.h"

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

TISP::SuperNode::superNodeVec *TISP_cascadeBiquad_createSuperNode(void            *pIn,
                                                                  void            *pOut,
                                                                  void            *pInputBlock,
                                                                  void            *pFilterCoeffBlock,
                                                                  void            *pFilterVarBlock,
                                                                  void            *pOutputBlock,
                                                                  size_t           inputSize,
                                                                  size_t           numChannels,
                                                                  size_t           numStages,
                                                                  uint32_t         biquadForm,
                                                                  size_t           dataPitchIn,
                                                                  size_t           dataPitchOut,
                                                                  size_t           filterVarPitch,
                                                                  bool             enableNchCoefs,
                                                                  uint32_t         dataType,
                                                                  TISP::ErrorCtxt &errorCtx)
{
   if (dataType == DSPLIB_FLOAT32) {
      const size_t stateRows = numStages * ((biquadForm == static_cast<uint32_t>(DSPLIB_BIQUAD_DF1))
                                                ? static_cast<size_t>(DSPLIB_NUM_STATE_VARS_DF1)
                                                : static_cast<size_t>(DSPLIB_NUM_STATE_VARS_DF2));

      const TISP::TISP_BufDesc2D inDesc    = {pInputBlock, TISP::Types::kFloat32, numChannels, 64U, dataPitchIn, 1U};
      const TISP::TISP_BufDesc2D outDesc   = {pOutputBlock, TISP::Types::kFloat32, numChannels, 64U, dataPitchOut, 1U};
      const TISP::TISP_BufDesc2D stateDesc = {pFilterVarBlock, TISP::Types::kFloat32, numChannels, stateRows,
                                              filterVarPitch,  1U};

      const TISP::TISP_BufDescList inputs  = {&inDesc, 1U};
      const TISP::TISP_BufDescList outputs = {&outDesc, 1U};
      const TISP::TISP_BufDescList states  = {&stateDesc, 1U};

      TISP::DSPLIB::CascadeBiquadConfig config;
      config.numStages       = numStages;
      config.biquadForm      = biquadForm;
      config.enableNChCoeffs = enableNchCoefs;

      TISP::DSPLIB::CascadeBiquadTunableParams<float> defaultParams;
      defaultParams.pCoeffs = static_cast<float *>(pFilterCoeffBlock);

      auto k0 = std::make_unique<TISP::DSPLIB::CascadeBiquad<float>>(&inputs, &outputs, &states, &config,
                                                                     &defaultParams, "CascadeBiquad", 0, errorCtx);

      if (!errorCtx.isSuccess()) {
         printf("\nError creating CascadeBiquad<float> node: %s\n", errorCtx.getMessage());
         return nullptr;
      }

      auto myOpVec = std::make_unique<TISP::opVec>();

      myOpVec->push_back(std::move(k0));

      auto s0 = std::make_unique<TISP::SuperNode::I2dToO2d<float, float>>(
          std::move(myOpVec), (float *) pIn, (float *) pOut, (float *) pInputBlock, (float *) pOutputBlock, numChannels,
          inputSize, numChannels, 64, dataPitchIn, dataPitchOut, (TOTAL_DMA_CHANNELS));

      auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

      mySuperNodeVec->push_back(std::move(s0));

      return mySuperNodeVec;
   }
   else {
      errorCtx.setInvalidParameterError("TISP_cascadeBiquad_createSuperNode",
                                        "unsupported data type for SuperNode creation");
      return nullptr;
   }
}

int TISP_cascadeBiquad_superNode_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t tpi;
   int32_t currentTestFail;
   int32_t fail = 0;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;

   TISP_cascadeBiquad_superNode_testParams_t *prm;
   TISP_cascadeBiquad_superNode_testParams_t  currPrm;
   TISP_cascadeBiquad_superNode_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_cascadeBiquad_superNode");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_cascadeBiquad_superNode.csv", "w+");
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

      size_t inputSize      = currPrm.dataSize;
      size_t dataPitchIn    = currPrm.dataPitchIn;
      size_t dataPitchOut   = currPrm.dataPitchOut;
      size_t filterSize     = currPrm.filterSize;
      size_t numChannels    = currPrm.numChannels;
      size_t datasize       = DSPLIB_sizeof(currPrm.dataType);
      size_t filterVarSize  = currPrm.filterVarSize;
      size_t numStages      = currPrm.numStages;
      size_t biquadForm     = currPrm.biquadForm;
      size_t enableNChCoefs = currPrm.enableNChCoefs;

      size_t outputSize = inputSize * numChannels * datasize;

      /* 2x buffer allocation for ping-pong buffers except state and filter buffer */
      auto pInputBlock       = myL2Heap.allocate64ByteAligned<uint8_t>(2 * 64 * numChannels * datasize);
      auto pFilterCoeffBlock = myL2Heap.allocate64ByteAligned<uint8_t>(filterSize * 1 * datasize);
      auto pFilterVarBlock   = myL2Heap.allocate64ByteAligned<uint8_t>(filterVarSize * numChannels * datasize);
      auto pOutputBlock      = myL2Heap.allocate64ByteAligned<uint8_t>(2 * 64 * numChannels * datasize);

      void *pOut = (void *) ddrBuffer;
      memset(pOut, 0, outputSize);

      /* Only run the test if the buffer allocations fit in the heap */
      if (pInputBlock.get() && pFilterCoeffBlock.get() && pFilterVarBlock.get() && pOutputBlock.get() && pOut) {

         DSPLIB_bufParams2D_t bufParamsFilterCoeff;
         DSPLIB_cascadeBiquad_get_coefficientsBufferDesc(currPrm.dataType, numStages, numChannels, biquadForm,
                                                         enableNChCoefs, &bufParamsFilterCoeff);

         TI_profile_start(TI_PROFILE_KERNEL_INIT);

         TI_fillBuffer_float(prm[tpi].testPattern, 0, pFilterCoeffBlock.get(), (void *) prm[tpi].filter,
                             bufParamsFilterCoeff.dim_x, bufParamsFilterCoeff.dim_y, bufParamsFilterCoeff.stride_y,
                             DSPLIB_sizeof(currPrm.dataType), testPatternString);

         // initialize state values to 0
         memset(pFilterVarBlock.get(), 0, filterVarSize * numChannels * datasize);

         TISP::ErrorCtxt                errorCtx;
         TISP::SuperNode::superNodeVec *mySuperNodeVec = TISP_cascadeBiquad_createSuperNode(
             (void *) currPrm.staticIn, pOut, pInputBlock.get(), pFilterCoeffBlock.get(), pFilterVarBlock.get(),
             pOutputBlock.get(), inputSize, numChannels, currPrm.numStages, currPrm.biquadForm, dataPitchIn,
             dataPitchOut, dataPitchIn, currPrm.enableNChCoefs, currPrm.dataType, errorCtx);

         if (!errorCtx.isSuccess()) {
            printf("\nError creating supernode: %s\n", errorCtx.getMessage());
            currentTestFail = 1;
            fail            = 1;
            continue;
         }

         TI_profile_stop();

         TI_profile_start(TI_PROFILE_KERNEL_OPT);

         // Execute the super node
         TISP::execute::graph(mySuperNodeVec);

         TI_profile_stop();

         // initialize state values to 0
         memset(pFilterVarBlock.get(), 0, filterVarSize * numChannels * datasize);

         /* The following for loop is to call kernel repeatedly so as to
          * train the branch predictor                                   */
         for (k = 0; k < NUM_REPS; k++) {

            TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

            // Execute the super node
            TISP::execute::graph(mySuperNodeVec);

            TI_profile_stop();
         }

         // initialize state values to 0
         memset(pFilterVarBlock.get(), 0, filterVarSize * numChannels * datasize);

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

         // Execute the super node
         TISP::execute::graph(mySuperNodeVec);

         TI_profile_stop();

         if (currPrm.staticOut != nullptr) {
            if (currPrm.dataType == DSPLIB_FLOAT32 || currPrm.dataType == DSPLIB_FLOAT64) {

               status_ref_vs_opt = TI_compare_mem_roi2DStatic_float(
                   (void *) pOut, (void *) currPrm.staticOut, 0, (double) 1e-4, numChannels, inputSize,
                   numChannels * datasize, DSPLIB_sizeof(currPrm.dataType));
            }
            else {
               status_ref_vs_opt =
                   TI_compare_mem_roi2DStatic((void *) pOut, (void *) currPrm.staticOut, 0, 0, numChannels, inputSize,
                                              numChannels * datasize, DSPLIB_sizeof(currPrm.dataType));
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

         sprintf(desc, "%s generated input | Data size=%d ", testPatternString, DSPLIB_sizeof(currPrm.dataType));

         // write to CSV, must happen prior to write to screen because
         // TI_profile_formula_add clears values in counters
         fprintf(fpOutputCSV,
                 "TISP_cascadeBiquad_superNode, %d, %ld,"
                 "%lu, %lu, %.2f, %d\n",
                 testNum, inputSize, archCycles, cycles[TI_PROFILE_KERNEL_OPT_WARM],
                 ((DSPLIB_F32) cycles[TI_PROFILE_KERNEL_OPT]) / ((DSPLIB_F32) archCycles), !currentTestFail);

         TI_profile_add_test(testNum++, currPrm.dataSize * currPrm.numChannels, 0, 0, currentTestFail, desc);

         delete mySuperNodeVec;
      }
      else {
         sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", DSPLIB_sizeof(currPrm.dataType),
                 currPrm.numChannels, currPrm.dataSize);
         TI_profile_skip_test(desc);
         // clear the counters between runs; normally handled by
         // TI_profile_add_test
         TI_profile_clear_run_stats();
      } // end of memory allocation successful
   }    // end idat test cases

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("_HOST_BUILD is defined.\n");
#endif
   return TISP_cascadeBiquad_superNode_d(&pProfile[0], 0);
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
