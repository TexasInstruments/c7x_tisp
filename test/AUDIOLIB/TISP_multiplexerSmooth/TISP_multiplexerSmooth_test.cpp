// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include AUDIOLIB
#include <algorithm>
#include <audiolib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <cstring>
#include <tisp.hpp>

// include test data for this kernel
#include "TISP_multiplexerSmooth_cn.h"
#include "TISP_multiplexerSmooth_idat.h"
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

std::unique_ptr<TISP::VirtualOp> TISP_multiplexerSmooth_createKernelObject(void            *pIn0,
                                                                           void            *pWs,
                                                                           void            *pSmoothedSampleBuff,
                                                                           void            *pGainFadeIn,
                                                                           void            *pGainFadeOut,
                                                                           void            *pOut,
                                                                           int32_t          maxInputs,
                                                                           int32_t          widthIn,
                                                                           int32_t          heightIn,
                                                                           int32_t          strideIn,
                                                                           int32_t          strideOut,
                                                                           uint32_t         currentIdx,
                                                                           uint32_t         isInterleave,
                                                                           uint32_t         sampleRate,
                                                                           uint32_t         dataType,
                                                                           float            fadeTime,
                                                                           TISP::ErrorCtxt &errorCtx)
{
   if (dataType == AUDIOLIB_FLOAT32) {
      return std::make_unique<TISP::AUDIOLIB::MultiplexerSmooth<float>>(
          (float *) pIn0, (float *) pSmoothedSampleBuff, (float *) pGainFadeIn, (float *) pGainFadeOut, (float *) pOut,
          (float *) pWs, maxInputs, widthIn, heightIn, strideIn, strideOut, isInterleave, currentIdx, sampleRate,
          fadeTime, errorCtx);
   }
   else if (dataType == AUDIOLIB_FLOAT64) {
      return std::make_unique<TISP::AUDIOLIB::MultiplexerSmooth<double>>(
          (double *) pIn0, (double *) pSmoothedSampleBuff, (double *) pGainFadeIn, (double *) pGainFadeOut,
          (double *) pOut, (double *) pWs, maxInputs, widthIn, heightIn, strideIn, strideOut, isInterleave, currentIdx,
          sampleRate, fadeTime, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_multiplexerSmooth_createKernelObject",
                                        "unsupported data type for Multiplexersmooth kernel");
      return nullptr;
   }
}

int TISP_multiplexerSmooth_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail     = 0;
   int32_t  eleCount = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t testNum;
   uint64_t archCycles = 0;
   uint64_t estCycles  = 0;

   TISP_multiplexerSmooth_testParams_t *prm;
   TISP_multiplexerSmooth_testParams_t  currPrm;
   TISP_multiplexerSmooth_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_multiplexerSmooth");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_multiplexerSmooth.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "widthIn, heightIn, Est cycles, EVM "
                        "cycles, EVM/Est cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);
   for (tpi = 0; tpi < test_cases; tpi++) {
      myL2Heap.reset();
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      for (repCount = 0; repCount < numReps; repCount++) {
         int32_t status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparij * strideIn + ison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;
         currentTestFail        = 0;
         size_t prevIdx         = 0;

         size_t dataSize = AUDIOLIB_sizeof(currPrm.dataType);
         size_t widthIn, heightIn, strideIn, heightOut, strideOut;
         size_t inp0SizeBytes, gainSizeBytes, outSizeBytes, smoothedSamplesSize;

         size_t  numBlocks          = currPrm.numBlocks;
         size_t  isInterleave       = currPrm.isInterleave;
         size_t  inSamples          = currPrm.inSamples;
         size_t  maxInputs          = currPrm.inputs;
         size_t  currentIdx         = currPrm.currentIdx;
         int32_t samplingRate       = currPrm.samplingRate;
         float   fadeTime           = currPrm.currentFadeTime;
         size_t  currentIdxCpy      = currentIdx;
         size_t  maxSmoothedSamples = int((currPrm.samplingRate * fadeTime) / 1000);

         if ((AUDIOLIB_sizeof(currPrm.dataType) == 4) || (AUDIOLIB_sizeof(currPrm.dataType) == 8)) {
            eleCount = (__C7X_VEC_SIZE_BYTES__ / AUDIOLIB_sizeof(currPrm.dataType));
         }

         if (isInterleave == 1) {
            widthIn  = currPrm.inChannels;
            heightIn = currPrm.inSamples;
            strideIn = currPrm.strideIn * dataSize;

            heightOut = currPrm.inSamples;
            strideOut = currPrm.strideOut * dataSize;

            if (heightIn <= maxSmoothedSamples) {
               maxSmoothedSamples = heightIn;
            }
         }
         else {
            widthIn  = currPrm.inSamples;
            heightIn = currPrm.inChannels;
            strideIn = currPrm.strideIn * dataSize;

            heightOut = currPrm.inChannels;
            strideOut = currPrm.strideOut * dataSize;

            if (widthIn <= maxSmoothedSamples) {
               maxSmoothedSamples = widthIn;
            }
         }

         size_t inputStride = strideIn * heightIn;

         size_t strideInElements  = strideIn / dataSize;
         size_t strideOutElements = strideOut / dataSize;

         inp0SizeBytes       = numBlocks * heightIn * strideIn * maxInputs;
         gainSizeBytes       = inSamples * dataSize;
         outSizeBytes        = numBlocks * heightOut * strideOut;
         smoothedSamplesSize = heightIn * maxSmoothedSamples * dataSize;
         smoothedSamplesSize = smoothedSamplesSize == 0 ? 5 : smoothedSamplesSize; /* for testing purpose */

         auto  pIn0                = myL2Heap.allocate64ByteAligned<uint8_t>(inp0SizeBytes);
         auto  pGainFadeIn         = myL2Heap.allocate64ByteAligned<uint8_t>(gainSizeBytes);
         auto  pGainFadeOut        = myL2Heap.allocate64ByteAligned<uint8_t>(gainSizeBytes);
         auto  pOut                = myL2Heap.allocate64ByteAligned<uint8_t>(outSizeBytes);
         auto  smoothedSamplesbuff = myL2Heap.allocate64ByteAligned<uint8_t>(smoothedSamplesSize);
         auto  pWs                 = myL2Heap.allocate64ByteAligned<uint8_t>(smoothedSamplesSize * 2);
         void *pOutCn              = (void *) ddrBuffer;

         memset(pOutCn, 0, outSizeBytes);
         memset((uint8_t *) ((void *) smoothedSamplesbuff.get()), 0,
                (smoothedSamplesSize *
                 2)); /* Allocate workspace memory for copying samples from input buffer and to get fader out*/
         memset((uint8_t *) ((void *) pWs.get()), 0, smoothedSamplesSize * 2);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn0.get() && pWs.get() && smoothedSamplesbuff.get() && pGainFadeIn.get() && pGainFadeOut.get() &&
             pOut.get() && pOutCn) {
            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               // fill input buffer
               TI_fillBuffer3D_float(prm[tpi].testPattern, 0, pIn0.get(), prm[tpi].staticIn0, widthIn, heightIn,
                                     maxInputs * numBlocks, strideIn, inputStride, dataSize, testPatternString);
            }
            else {
               TI_fillBuffer3D(prm[tpi].testPattern, 0, pIn0.get(), prm[tpi].staticIn0, widthIn, heightIn,
                               maxInputs * numBlocks, strideIn, inputStride, dataSize, testPatternString);
            }

            TI_fillBuffer_float(prm[tpi].testPattern, 0, pGainFadeIn.get(), prm[tpi].staticIn1, inSamples, 1, 0,
                                AUDIOLIB_sizeof(currPrm.dataType), testPatternString);

            TI_fillBuffer_float(prm[tpi].testPattern, 0, pGainFadeOut.get(), prm[tpi].staticIn2, inSamples, 1, 0,
                                AUDIOLIB_sizeof(currPrm.dataType), testPatternString);

            // initialize set function parameters
            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_multiplexerSmooth_createKernelObject(
                           pIn0.get(), pWs.get(), smoothedSamplesbuff.get(), pGainFadeIn.get(), pGainFadeOut.get(), pOut.get(),
                           static_cast<uint32_t>(maxInputs), static_cast<uint32_t>(widthIn), static_cast<uint32_t>(heightIn),
                           static_cast<int32_t>(strideIn), static_cast<int32_t>(strideOut), static_cast<uint32_t>(currentIdx),
                           static_cast<uint8_t>(isInterleave), static_cast<int32_t>(samplingRate), currPrm.dataType,
                           static_cast<float>(fadeTime), errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            TI_profile_stop();

            uint16_t paramId = static_cast<uint16_t>(TISP::AUDIOLIB::MultiplexersmoothParamId::kSrcIndex);

            /* Prepare run time control settings */
            size_t              step = (int) (numBlocks / 5);
            std::vector<size_t> changeTime;
            for (size_t v = 1; v < numBlocks; v += step) {
               changeTime.push_back(v);
            }
            std::vector<int> chnageIdx = {(int) maxInputs - 1, (int) static_cast<int>((maxInputs - 1) / 2),
                                          (int) maxInputs, -1, 0};

            for (size_t n = 0; n < numBlocks; n++) {

               /* Switch scrIdx in run time */
               auto it = std::find(changeTime.begin(), changeTime.end(), n);
               if (it != changeTime.end()) {
                  std::size_t idx = static_cast<std::size_t>(it - changeTime.begin());
                  k0->setParams(paramId, &chnageIdx[idx], sizeof(chnageIdx[idx]), errorCtx);
                  if (!errorCtx.isSuccess()) {
                     if (chnageIdx[idx] >= 0 && chnageIdx[idx] < (int) maxInputs) {
                        printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                     }
                     errorCtx.reset();
                  }
               }

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn0.get() + (n * maxInputs * inputStride);
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightIn * strideOut;

               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            memset((void *) smoothedSamplesbuff.get(), 0, smoothedSamplesSize);
            currentIdx = (currentIdx >= maxInputs) ? 0 : currentIdxCpy;
            prevIdx    = currentIdx;

            for (size_t n = 0; n < numBlocks; n++) {
               /* Switch scrIdx in run time */
               auto it = std::find(changeTime.begin(), changeTime.end(), n);
               if (it != changeTime.end()) {
                  std::size_t idx = static_cast<std::size_t>(it - changeTime.begin());
                  currentIdx      = chnageIdx[idx];
               }

               AUDIOLIB_multiplexerSmooth_cn(pIn0.get() + n * maxInputs * inputStride, smoothedSamplesbuff.get(),
                                             pGainFadeOut.get(), pGainFadeIn.get(),
                                             ((uint8_t *) pOutCn + n * heightIn * strideOut), widthIn, heightIn,
                                             maxInputs, strideInElements, strideOutElements, &prevIdx, &currentIdx,
                                             maxSmoothedSamples, isInterleave, currPrm.dataType);
               prevIdx = currentIdx;
            }

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            for (size_t n = 0; n < numBlocks; n++) {
               if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                  status_nat_vs_opt &=
                      TI_compare_mem_2D_float((void *) ((uint8_t *) pOut.get() + n * heightIn * strideOut),
                                              (void *) ((uint8_t *) pOutCn + n * heightIn * strideOut), 0,
                                              (double) powf(2, -10), widthIn, heightIn, strideOut, dataSize);
               }
            }

            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               for (size_t n = 0; n < numBlocks; n++) {
                  if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                     status_ref_vs_opt &= TI_compare_mem_2DDualStride_float(
                         (void *) ((uint8_t *) pOut.get() + n * heightIn * strideOut),
                         (void *) ((uint8_t *) currPrm.staticOut + n * widthIn * heightIn * dataSize), 0,
                         (double) powf(2, -10), widthIn, heightIn, strideOut, widthIn * dataSize, dataSize);
                  }
               }
            }
            else {
               /* Set to pass since it wasn't supposed to run. */
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
            }

            /* k0->perfEst(&setEstParams); */

            for (uint32_t k = 0; k < NUM_REPS; k++) {

               TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

               currentIdx = (currentIdxCpy >= maxInputs) ? 0 : currentIdxCpy;
               prevIdx    = currentIdx;

               k0->setParams(paramId, &currentIdx, sizeof(currentIdx), errorCtx);
               if (!errorCtx.isSuccess()) {
                  printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                  errorCtx.reset();
               }
               k0->exec();
               TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
               for (size_t n = 0; n < numBlocks; n++) {

                  /* Switch scrIdx in run time */
                  auto it = std::find(changeTime.begin(), changeTime.end(), n);
                  if (it != changeTime.end()) {
                     std::size_t idx = static_cast<std::size_t>(it - changeTime.begin());
                     k0->setParams(paramId, &chnageIdx[idx], sizeof(chnageIdx[idx]), errorCtx);
                     if (!errorCtx.isSuccess()) {
                        if (chnageIdx[idx] >= 0 && chnageIdx[idx] < (int) maxInputs) {
                           printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                        }
                        errorCtx.reset();
                     }
                  }

                  // set address for block to process
                  kAddrMode    = TISP::kAddrModeInOut;
                  kAddr.src[0] = (uint8_t *) pIn0.get() + (n * maxInputs * inputStride);
                  kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightIn * strideOut;

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
            for (uint32_t k = 0; k < outSizeBytes; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            // dummy store of outSum to insure that the compiler does not remove it.
            volatileSum = outSum;

            currentIdx = (currentIdxCpy >= maxInputs) ? 0 : currentIdxCpy;

            k0->setParams(paramId, &currentIdx, sizeof(currentIdx), errorCtx);
            if (!errorCtx.isSuccess()) {
               printf("\nError setting parameters: %s\n", errorCtx.getMessage());
               errorCtx.reset();
            }
            k0->exec();

            for (size_t n = 0; n < numBlocks; n++) {

               /* Switch scrIdx in run time */
               auto it = std::find(changeTime.begin(), changeTime.end(), n);
               if (it != changeTime.end()) {
                  std::size_t idx = static_cast<std::size_t>(it - changeTime.begin());
                  k0->setParams(paramId, &chnageIdx[idx], sizeof(chnageIdx[idx]), errorCtx);
                  if (!errorCtx.isSuccess()) {
                     if (chnageIdx[idx] >= 0 && chnageIdx[idx] < (int) maxInputs) {
                        printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                     }
                     errorCtx.reset();
                  }
               }

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn0.get() + (n * maxInputs * inputStride);
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightIn * strideOut;

               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            currentTestFail =
                ((status_nat_vs_opt == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) ||
                 (comparisonDone == 0) || (currentTestFail == 1))
                    ? 1
                    : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString, AUDIOLIB_sizeof(currPrm.dataType));

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters

            fprintf(fpOutputCSV,
                    "TISP_multiplexerSmooth, %d, %d, "
                    "%d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.inSamples, currPrm.inChannels, (unsigned long) estCycles,
                    pProfile[3 * tpi + 1], ((float) cycles[TI_PROFILE_KERNEL_OPT_WARM]) / ((float) archCycles),
                    !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.inSamples * currPrm.inChannels, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, widthIn size=%d, heightInIn size=%d", AUDIOLIB_sizeof(currPrm.dataType),
                    currPrm.inSamples, currPrm.inChannels);
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
   return TISP_multiplexerSmooth_d(&pProfile[0], 0);
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
   return 0;
}

#ifdef __cplusplus
}
#endif
