// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include DSPLIB
#include <cstdint>
#include <dsplib.h>

// include test infrastructure provided by DSPLIB
#include "../common/TISP_test.h"
#include "../common/TISP_unit_test_l2_heap.hpp"

// include test data for this kernel
#include "TISP_cascadeBiquad.hpp"
#include "TISP_cascadeBiquad_idat.hpp"
#include "common/DSPLIB_types.h"
#include <TISP_ErrorCtxt.hpp>

#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#if defined(__C7504__) || defined(__C7524__)

__attribute__((section(".l2sramaux"), aligned(64))) int8_t  l2auxBuffer[TISP_L2_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t    ddrBuffer[2048 * 1024];
__attribute__((section(".l2sramData"), aligned(64))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];

#else
__attribute__((section(".msmcData"), aligned(64))) int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t  ddrBuffer[2048 * 1024];
#endif

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

int DSPLIB_cascadeBiquad_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t              tpi;
   int32_t              currentTestFail;
   int32_t              fail = 0;
   uint32_t             repCount;
   uint32_t             numReps;
   DSPLIB_bufParams2D_t bufParamsIn, bufParamsOut, bufParamsFilterVar;
   DSPLIB_bufParams2D_t bufParamsFilterCoeff;   // coefficient buffer descriptor for optimized code
   DSPLIB_bufParams2D_t bufParamsFilterCoeffCn; // coefficient buffer descriptor for natural-C code

   uint32_t k = 0;

   uint32_t testNum;
   uint64_t archCycles, estCycles = 0;

   DSPLIB_cascadeBiquad_testParams_t *prm;
   DSPLIB_cascadeBiquad_testParams_t  currPrm;
   DSPLIB_cascadeBiquad_getTestParams(&prm, &test_cases);

   DSPLIB_cascadeBiquad_InitArgs kerInitArgs;

#if DSPLIB_DEBUGPRINT
   printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 0\n");
#endif

   int32_t             handleSize = DSPLIB_cascadeBiquad_getHandleSize(&kerInitArgs);
   DSPLIB_kernelHandle handle     = malloc(handleSize);

#if DSPLIB_DEBUGPRINT
   printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 1\n");
#endif

   TI_profile_init("DSPLIB_cascadeBiquad");

   FILE *fpOutputCSV = fopen("DSPLIB_cascadeBiquad.csv", "w+");
   fprintf(fpOutputCSV,
           "Kernel, Test ID, Test type, Bit Width, Parameters, dataSize, numChannels, numStages, "
           "biquadForm, enableNchCoeffs, Arch cycles, Estimated cycles, Warm Cache cycles, Est/Warm cycles, "
           "Pass/Fail, Cold Cycles, Warm Cycles, Warm Cycles WRB\n");

   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {

      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      /* printf("\nmyL2Heap._offset outside:%ld\n", myL2Heap._offset); */
      for (repCount = 0; repCount < numReps; repCount++) {
         /* printf("\nmyL2Heap._offset:%ld\n", myL2Heap._offset); */
         int32_t       status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t       status_ref_vs_opt = TI_TEST_KERNEL_FAIL;
         DSPLIB_STATUS status_coefs      = DSPLIB_ERR_FAILURE;
         DSPLIB_STATUS status_init_opt   = DSPLIB_ERR_FAILURE;
         DSPLIB_STATUS status_exec_opt   = DSPLIB_ERR_FAILURE;
         DSPLIB_STATUS status_init_nat   = DSPLIB_ERR_FAILURE;
         DSPLIB_STATUS status_exec_nat   = DSPLIB_ERR_FAILURE;
         DSPLIB_STATUS status            = DSPLIB_ERR_FAILURE;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;

         currentTestFail = 0;

         kerInitArgs.dataSize        = currPrm.dataSize;
         kerInitArgs.numChannels     = currPrm.numChannels;
         kerInitArgs.numStages       = currPrm.numStages;
         kerInitArgs.enableNChCoeffs = currPrm.enableNChCoeffs;
         kerInitArgs.biquadForm      = currPrm.biquadForm;

         bufParamsIn.data_type = currPrm.dataType;
         bufParamsIn.dim_x     = currPrm.numChannels;
         bufParamsIn.dim_y     = currPrm.dataSize;
         bufParamsIn.stride_y  = currPrm.dataPitch;

         bufParamsOut.data_type = currPrm.dataType;
         bufParamsOut.dim_x     = currPrm.numChannels;
         bufParamsOut.dim_y     = currPrm.dataSize;
         bufParamsOut.stride_y  = currPrm.dataPitch;

         bufParamsFilterVar.data_type = currPrm.dataType;
         bufParamsFilterVar.dim_x     = currPrm.numChannels;
         bufParamsFilterVar.dim_y     = (currPrm.biquadForm == DSPLIB_BIQUAD_DF1)
                                            ? currPrm.numStages * DSPLIB_NUM_STATE_VARS_DF1
                                            : currPrm.numStages * DSPLIB_NUM_STATE_VARS_DF2;
         bufParamsFilterVar.stride_y  = currPrm.filterVarPitch;

         // Configure Natural-C coefficient buffer descriptor
         // Coefficients are arranged by channel, stage, and coefficient
         bufParamsFilterCoeffCn.data_type = currPrm.dataType;
         bufParamsFilterCoeffCn.dim_x     = currPrm.numStages * DSPLIB_NUM_BIQUAD_COEFS;
         bufParamsFilterCoeffCn.dim_y     = (currPrm.enableNChCoeffs == 0) ? 1 : currPrm.numChannels;
         bufParamsFilterCoeffCn.stride_y =
             bufParamsFilterCoeffCn.dim_x * DSPLIB_sizeof(bufParamsFilterCoeffCn.data_type);

         // Get optimized coefficient buffer descriptor
         status_coefs = DSPLIB_cascadeBiquad_get_coefficientsBufferDesc(currPrm.dataType, currPrm.numStages,
                                                                        currPrm.numChannels, currPrm.biquadForm,
                                                                        currPrm.enableNChCoeffs, &bufParamsFilterCoeff);
         if (status_coefs != DSPLIB_SUCCESS) {
            sprintf(TI_test_print_buffer, "\ntestNum %d: DSPLIB_cascadeBiquad_get_coefficientsBufferDesc FAIL\n",
                    testNum);
            TI_test_print_string(TI_test_print_buffer);
            break;
         }

         auto pIn = myL2Heap.allocate64ByteAligned<uint8_t>(bufParamsIn.stride_y * bufParamsIn.dim_y);

         auto pFilterCoeff =
             myL2Heap.allocate64ByteAligned<uint8_t>(bufParamsFilterCoeff.dim_y * bufParamsFilterCoeff.stride_y);

         void *pFilterCoeffCn = malloc(bufParamsFilterCoeffCn.dim_y * bufParamsFilterCoeffCn.stride_y);

         void *pFilterCoeffTemp = malloc(bufParamsFilterCoeff.dim_y * bufParamsFilterCoeff.stride_y);

         auto pFilterVar =
             myL2Heap.allocate64ByteAligned<uint8_t>(bufParamsFilterVar.stride_y * bufParamsFilterVar.dim_y);

         void *pFilterVarCn = malloc(bufParamsFilterVar.stride_y * bufParamsFilterVar.dim_y);

         void *pFilterVarTemp = malloc(bufParamsFilterVar.stride_y * bufParamsFilterVar.dim_y);

         auto  pOut   = myL2Heap.allocate64ByteAligned<uint8_t>(bufParamsOut.dim_y * bufParamsOut.stride_y);
         void *pOutCn = (void *) ddrBuffer;

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pFilterCoeff.get() && pFilterVar.get() && pOut.get() && pFilterCoeffCn && pFilterVarCn &&
             pOutCn && pFilterCoeffTemp && pFilterVarTemp) {

#if DSPLIB_DEBUGPRINT
            printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d pIn %p pFilterCoeff %p pFilterCoeffCn %p pFilterVar %p "
                   "pFilterVarCn %p pOut %p pOutCn %p\n",
                   pIn, pFilterCoeff, pFilterCoeffCn, pFilterVar, pFilterVarCn, pOut, pOutCn);
#endif

            /* Set up the test patterns */

            // Initialize input buffer
            TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn.get(), prm[tpi].staticIn, bufParamsIn.dim_x,
                                bufParamsIn.dim_y, bufParamsIn.stride_y, sizeof(DSPLIB_F32), testPatternString);

            // Initialize Natural-C coefficient buffer
            TI_fillBuffer_float(prm[tpi].testPattern, 0, pFilterCoeffCn, prm[tpi].staticFilterCoeff,
                                bufParamsFilterCoeffCn.dim_x, bufParamsFilterCoeffCn.dim_y,
                                bufParamsFilterCoeffCn.stride_y, sizeof(DSPLIB_F32), testPatternString);

            // Initialize optimized coefficient buffer
            if (prm[tpi].enableCalcCoeffs == 0) {
               // Initialize coefficients with static values

               if ((prm[tpi].biquadForm == DSPLIB_BIQUAD_TIF) || (prm[tpi].enableNChCoeffs == 1)) {
                  // Using pre-calculated optimized coefficients
                  TI_fillBuffer_float(prm[tpi].testPattern, 0, pFilterCoeff.get(), prm[tpi].staticFilterCoeffOpt,
                                      bufParamsFilterCoeff.dim_x, bufParamsFilterCoeff.dim_y,
                                      bufParamsFilterCoeff.stride_y, sizeof(DSPLIB_F32), testPatternString);
               }
               else {
                  // Natural-C and optimized coefficients are the same
                  TI_fillBuffer_float(prm[tpi].testPattern, 0, pFilterCoeff.get(), prm[tpi].staticFilterCoeff,
                                      bufParamsFilterCoeff.dim_x, bufParamsFilterCoeff.dim_y,
                                      bufParamsFilterCoeff.stride_y, sizeof(DSPLIB_F32), testPatternString);
               }
            }
            else { // (prm[tpi].enableCalcCoeffs == 1)
               // Get optimized coefficients using helper function
               status_coefs = DSPLIB_cascadeBiquad_get_coefficients(currPrm.biquadForm, currPrm.enableNChCoeffs,
                                                                    &bufParamsFilterCoeffCn, &bufParamsFilterCoeff,
                                                                    pFilterCoeffCn, pFilterCoeff.get());
               if (status_coefs != DSPLIB_SUCCESS) {
                  sprintf(TI_test_print_buffer, "\ntestNum %d: DSPLIB_cascadeBiquad_get_coefficients FAIL\n", testNum);
                  TI_test_print_string(TI_test_print_buffer);
                  break;
               }
            }

            memset(pFilterVar.get(), 0, bufParamsFilterVar.dim_y * bufParamsFilterVar.stride_y);
            memcpy(pFilterVarCn, pFilterVar.get(), bufParamsFilterVar.dim_y * bufParamsFilterVar.stride_y);
            memcpy(pFilterVarTemp, pFilterVar.get(), bufParamsFilterVar.dim_y * bufParamsFilterVar.stride_y);
            memcpy(pFilterCoeffTemp, pFilterCoeff.get(), bufParamsFilterCoeff.dim_y * bufParamsFilterCoeff.stride_y);

#if DSPLIB_DEBUGPRINT
            printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 0\n");
#endif

            kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;
            status_init_opt       = DSPLIB_cascadeBiquad_init_checkParams(handle, &bufParamsIn, &bufParamsFilterCoeff,
                                                                          &bufParamsFilterVar, &bufParamsOut, &kerInitArgs);

#if DSPLIB_DEBUGPRINT
            printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 1 status_init %d\n", status_init);
#endif
            TISP::ErrorCtxt errorCtx;
            auto            k0 = std::make_unique<TISP::DSPLIB::CascadeBiquad<float>>(
                (float *) pIn.get(), (float *) pFilterCoeff.get(), (float *) pFilterVar.get(), (float *) pOut.get(),
                static_cast<uint32_t>(currPrm.dataSize), static_cast<uint32_t>(currPrm.numChannels),
                static_cast<uint32_t>(currPrm.numStages), static_cast<uint32_t>(currPrm.biquadForm),
                static_cast<uint32_t>(currPrm.dataPitch), static_cast<uint32_t>(currPrm.dataPitch),
                static_cast<uint32_t>(currPrm.filterVarPitch), static_cast<uint32_t>(currPrm.enableNChCoeffs),
                errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            status_init_opt = DSPLIB_SUCCESS;
            /* if (status_init_opt == DSPLIB_SUCCESS) { */
            /*    TI_profile_start(TI_PROFILE_KERNEL_INIT); */
            /*    DSPLIB_asm(" MARK 0"); */
            /*    status_init_opt = DSPLIB_cascadeBiquad_init(handle, &bufParamsIn, &bufParamsFilterCoeff, */
            /*                                                &bufParamsFilterVar, &bufParamsOut, &kerInitArgs); */
            /*    status_init_opt = DSPLIB_SUCCESS; */

            /*    DSPLIB_asm(" MARK 1"); */
            /*    TI_profile_stop(); */
            /* } */
#if DSPLIB_DEBUGPRINT
            printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 2 status_init %d\n", status_init);
#endif

            /* if (status_init_opt == DSPLIB_SUCCESS) */ {
               status_exec_opt = DSPLIB_cascadeBiquad_exec_checkParams(handle, pIn.get(), pFilterCoeff.get(),
                                                                       pFilterVar.get(), pOut.get());
#if DSPLIB_DEBUGPRINT
               printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 3 status_opt %d\n", status_opt);
#endif
               if (status_exec_opt == DSPLIB_SUCCESS) {
                  TI_profile_start(TI_PROFILE_KERNEL_OPT);
                  DSPLIB_asm(" MARK 2");
                  /* status_exec_opt = DSPLIB_cascadeBiquad_exec(handle, pIn, pFilterCoeff, pFilterVar, pOut); */
                  k0->exec();
                  DSPLIB_asm(" MARK 3");
                  TI_profile_stop();
               }
            }
            status = ((status_init_opt == DSPLIB_SUCCESS) && (status_exec_opt == DSPLIB_SUCCESS)) ? DSPLIB_SUCCESS
                                                                                                  : DSPLIB_ERR_FAILURE;

            if (status == DSPLIB_SUCCESS) {
               /* The following for loop is to call kernel repeatedly so as to
                * train the branch predictor                                   */
               memcpy(pFilterVar.get(), pFilterVarTemp, bufParamsFilterVar.dim_y * bufParamsFilterVar.stride_y);
               memcpy(pFilterCoeff.get(), pFilterCoeffTemp, bufParamsFilterCoeff.dim_y * bufParamsFilterCoeff.stride_y);
               status_init_opt = DSPLIB_cascadeBiquad_init(handle, &bufParamsIn, &bufParamsFilterCoeff,
                                                           &bufParamsFilterVar, &bufParamsOut, &kerInitArgs);

               if (status_init_opt == DSPLIB_SUCCESS) {
                  //	    for (k = 0; k < 4; k++) {
                  // run warm instruction cache test
                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);
                  DSPLIB_asm(" MARK 4");
                  k0->exec();
                  /* status_exec_opt = DSPLIB_cascadeBiquad_exec(handle, pIn, pFilterCoeff, pFilterVar, pOut); */
                  DSPLIB_asm(" MARK 5");
                  TI_profile_stop();
                  //	    }
               }
            }
            status = ((status_init_opt == DSPLIB_SUCCESS) && (status_exec_opt == DSPLIB_SUCCESS)) ? DSPLIB_SUCCESS
                                                                                                  : DSPLIB_ERR_FAILURE;

            if (status == DSPLIB_SUCCESS) {
               // get output to L1D
               int16_t outSum   = 0;
               int8_t *pOutTemp = (int8_t *) pOut.get(); // treat output as bytes to be data type agnostic
               for (k = 0; k < bufParamsOut.dim_x; k++) {
                  outSum += *pOutTemp;
                  pOutTemp++;
               }

               // dummy store of outSum to insure that the compiler does not remove it.
               memcpy(pFilterVar.get(), pFilterVarTemp, bufParamsFilterVar.dim_y * bufParamsFilterVar.stride_y);
               memcpy(pFilterCoeff.get(), pFilterCoeffTemp, bufParamsFilterCoeff.dim_y * bufParamsFilterCoeff.stride_y);

               volatileSum = outSum;

               /* status_init_opt = DSPLIB_cascadeBiquad_init(handle, &bufParamsIn,
&bufParamsFilterCoeff, */
               /*                                             &bufParamsFilterVar, &bufParamsOut,
&kerInitArgs); */

               /* auto k0 = std::make_unique<TISP::DSPLIB::CascadeBiquad<float>>( */
               /*     pIn, pFilterCoeff, pFilterVar, pOut, currPrm.dataSize, currPrm.numChannels, currPrm.numStages, */
               /*     currPrm.biquadForm, currPrm.dataPitch, currPrm.dataPitch, currPrm.filterVarPitch, */
               /*     currPrm.enableNChCoeffs); */
               status_init_opt = DSPLIB_SUCCESS;
               if (status_init_opt == DSPLIB_SUCCESS) {
                  // run warm instruction cache test
                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

                  DSPLIB_asm(" MARK 6");
                  /* status_exec_opt = DSPLIB_cascadeBiquad_exec(handle, pIn, pFilterCoeff, pFilterVar, pOut); */
                  k0->exec();
                  DSPLIB_asm(" MARK 7");
                  TI_profile_stop();

#if DSPLIB_DEBUGPRINT
                  printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 4 status_opt %d\n", status_opt);
#endif
               }
            }
            status = ((status_init_opt == DSPLIB_SUCCESS) && (status_exec_opt == DSPLIB_SUCCESS)) ? DSPLIB_SUCCESS
                                                                                                  : DSPLIB_ERR_FAILURE;

            if (status == DSPLIB_SUCCESS) {
               /* Test _cn kernel */
               kerInitArgs.funcStyle = DSPLIB_FUNCTION_NATC;
               status_init_nat       = DSPLIB_cascadeBiquad_init_checkParams(
                         handle, &bufParamsIn, &bufParamsFilterCoeffCn, &bufParamsFilterVar, &bufParamsOut, &kerInitArgs);

               if (status_init_nat == DSPLIB_SUCCESS) {
                  // initialize the kernel to use the natural C variant
                  status_init_nat = DSPLIB_cascadeBiquad_init(handle, &bufParamsIn, &bufParamsFilterCoeffCn,
                                                              &bufParamsFilterVar, &bufParamsOut, &kerInitArgs);
               }

               if (status_init_nat == DSPLIB_SUCCESS) {
#if DSPLIB_DEBUGPRINT
                  printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 5\n");
#endif
                  TI_profile_start(TI_PROFILE_KERNEL_CN);
                  DSPLIB_asm(" MARK 8");
                  status_exec_nat = DSPLIB_cascadeBiquad_exec(handle, pIn.get(), pFilterCoeffCn, pFilterVarCn, pOutCn);
                  DSPLIB_asm(" MARK 9");
                  TI_profile_stop();
#if DSPLIB_DEBUGPRINT
                  printf("DSPLIB_DEBUGPRINT  DSPLIB_cascadeBiquad_d CP 6 status_nat %d pOut %p\n", status_nat,
                         pOut.get());
#endif
               }
            }

            if ((status_init_nat == DSPLIB_SUCCESS) && (status_exec_nat == DSPLIB_SUCCESS)) {
               /* Compare Natural-C and Optimized outputs */
               status_nat_vs_opt = TI_compare_mem_2D_float(
                   (void *) pOut.get(), (void *) pOutCn, 0, (double) powf(2, -10), bufParamsOut.dim_x,
                   bufParamsOut.dim_y, bufParamsOut.stride_y, sizeof(DSPLIB_F32));

               comparisonDone = 1;
            }

            if ((status_init_opt == DSPLIB_SUCCESS) && (status_exec_opt == DSPLIB_SUCCESS)) {
               /* Compare Reference and Optimized outputs */
               if (currPrm.staticOut != nullptr) {
                  status_ref_vs_opt = TI_compare_mem_roi2DStatic_float(
                      (void *) pOut.get(), (void *) currPrm.staticOut, 0, (double) powf(2, -10), bufParamsOut.dim_x,
                      bufParamsOut.dim_y, bufParamsOut.stride_y, sizeof(DSPLIB_F32));

                  comparisonDone = 1;
               }
               else {
                  /* Set to pass since it wasn't supposed to run. */
                  status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               }
            }

            /* Set the 'fail' flag based on test vector comparison results */
            currentTestFail = ((status_nat_vs_opt == TI_TEST_KERNEL_FAIL) ||
                               (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) || (status_init_opt != DSPLIB_SUCCESS) ||
                               (status_exec_opt != DSPLIB_SUCCESS) || (status_init_nat != DSPLIB_SUCCESS) ||
                               (status_exec_nat != DSPLIB_SUCCESS) || (comparisonDone == 0) || (currentTestFail == 1))
                                  ? 1
                                  : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            sprintf(desc,
                    "%s generated input | Data size = %d, numChannels = %d, numStages = %d, biquadForm = %d, "
                    "enableNChCoeffs = %d",
                    testPatternString, currPrm.dataSize, currPrm.numChannels, currPrm.numStages, currPrm.biquadForm,
                    currPrm.enableNChCoeffs);

            // Calculate estimated cycles
            DSPLIB_cascadeBiquad_perfEst(handle, &bufParamsIn, &bufParamsFilterCoeff, &bufParamsFilterVar,
                                         &bufParamsOut, &archCycles, &estCycles);

            // Write to CSV, must happen prior to write to screen because TI_profile_formula_add clears values in
            // counters
            fprintf(fpOutputCSV,
                    "cascadeBiquad, %d, %d, %d, Data size:%d numChannels:%d numStages:%d biquadForm:%d "
                    "enableNChCoeffs:%d, %d, %d, %d, %d, %d, %lu, %lu, %lu, %.2f, %d, "
                    "%d, %d, %d\n",
                    testNum, currPrm.testPattern, DSPLIB_sizeof(bufParamsIn.data_type) * 8, currPrm.dataSize,
                    currPrm.numChannels, currPrm.numStages, currPrm.biquadForm, currPrm.enableNChCoeffs,
                    currPrm.dataSize, currPrm.numChannels, currPrm.numStages, currPrm.biquadForm,
                    currPrm.enableNChCoeffs, archCycles, estCycles, cycles[TI_PROFILE_KERNEL_OPT_WARM],
                    ((DSPLIB_F32) estCycles) / ((DSPLIB_F32) cycles[TI_PROFILE_KERNEL_OPT_WARM]), !currentTestFail,
                    pProfile[3 * tpi], pProfile[3 * tpi + 1], pProfile[3 * tpi + 2]);

            TI_profile_add_test(testNum++, currPrm.dataSize * currPrm.numChannels * currPrm.numStages, archCycles,
                                estCycles, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "dataSize=%d, numChannels=%d, numStages=%d, biquadForm=%d, enableNChCoeffs=%d",
                    currPrm.dataSize, currPrm.numChannels, currPrm.numStages, currPrm.biquadForm,
                    currPrm.enableNChCoeffs);
            TI_profile_skip_test(desc);
            // clear the counters between runs; normally handled by TI_profile_add_test
         } // end of memory allocation successful?

         /* Free buffers for each test vector */
         /* if (currPrm.outputDataLocation == DSPLIB_TEST_OUTPUT_HEAP) { */
         /* free(pOutCn); */
         /* TI_align_free(pOut); */
         /* } */

         /* TI_align_free(pIn); */
         /* TI_align_free(pFilterVar); */
         free(pFilterVarCn);
         free(pFilterVarTemp);
         /* TI_align_free(pFilterCoeff); */
         free(pFilterCoeffCn);
         free(pFilterCoeffTemp);
      } // end repetitions
   }    // end idat test cases

   free(handle);

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   /* if (TI_cache_init()) { */
   /*    TI_memError("DSPLIB_cascadeBiquad"); */
   /*    return 1; */
   /* } */
   /* else */
#else
   printf("_HOST_BUILD is defined.\n");
#endif
   {
      return DSPLIB_cascadeBiquad_d(&pProfile[0], 0);
   }
}

#ifdef __cplusplus
extern "C" {
#endif
/* Main call for individual test projects */
#if !defined(__ONESHOTTEST) && !defined(RTL_TEST)
int tisp_main()
{
   int fail = 1;

   uint32_t profile[1024 * 3]; // up to 1024 test supported

   /* DSPLIB_TEST_init(); */

   fail = test_main(&profile[0]);

#if !defined(NO_PRINTF)
   if (fail == 0)
      printf("Test Pass!\n");
   else
      printf("Test Fail!\n");

   int i;
   for (i = 0; i < test_cases; i++) {
      printf("Test %4d: Cold Cycles = %8d, Warm Cycles = %8d, Warm Cycles WRB = %8d\n", i, profile[3 * i],
             profile[3 * i + 1], profile[3 * i + 2]);
   }
#endif

   if (fail == 0) {
      printf("Test Pass!\n");
   }
   else {
      printf("Test Fail!\n");
   }

   return fail;
}
#endif
#ifdef __cplusplus
}
#endif
