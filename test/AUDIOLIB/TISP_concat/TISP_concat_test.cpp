// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include AUDIOLIB
#include "../common/TISP_test.h"
#include "../common/TISP_unit_test_l2_heap.hpp"
#include "../common/TI_profile.h"
#include "TISP_concat.hpp"
#include "TISP_concat_cn.h"
#include "TISP_concat_idat.h"
#include <audiolib.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <tisp.hpp>
#include <vector>
#include <TISP_ErrorCtxt.hpp>

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024)
#elif SOC == AM62A || SOC == AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024)
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (1024 * 1024)
#else
#define L2SRAM_HEAP_SIZE (1024 * 1024)
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
#endif

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

std::unique_ptr<TISP::VirtualOp> TISP_concat_createKernelObject(TISP::TISP_BufDesc2D *concatInput,
                                                                void                 *pOut,
                                                                uint32_t              numInputs,
                                                                uint32_t              inSamples,
                                                                uint8_t               isInterleave,
                                                                size_t                outPitchBytes,
                                                                uint32_t              sampleDataType,
                                                                TISP::ErrorCtxt      &errorCtx)
{
   if (sampleDataType == AUDIOLIB_FLOAT32) {
      return std::make_unique<TISP::AUDIOLIB::Concat<float>>(concatInput, numInputs, (float *) pOut, outPitchBytes,
                                                             inSamples, isInterleave, errorCtx);
   }
   else if (sampleDataType == AUDIOLIB_FLOAT64) {
      return std::make_unique<TISP::AUDIOLIB::Concat<double>>(concatInput, numInputs, (double *) pOut, outPitchBytes,
                                                              inSamples, isInterleave, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_concat_createKernelObject", "unsupported data type for Concat kernel");
      return nullptr;
   }
}

int TISP_concat_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
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

   TISP_concat_testParams_t *prm;
   TISP_concat_testParams_t  currPrm;
   TISP_concat_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_concat");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_concat.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "WidthIn, HeightIn, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (int tpi = 0; tpi < test_cases; tpi++) {
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
         currentTestFail        = 0;
         int32_t eleCount       = 0;

         constexpr size_t kMaxNumInputs = static_cast<size_t>(TISP::kMaxSrcAddr); // SetAddr_t src[] capacity

         size_t dataSize        = AUDIOLIB_sizeof(currPrm.dataType);
         size_t inSamples       = currPrm.inSamples;
         size_t numInputs       = currPrm.numInputs;
         size_t totalInChannels = currPrm.totalInChannels;

         // Per-input channel counts + tight per-input pitches (bytes & elements)
         uint32_t inChannelsArr[kMaxNumInputs]   = {0};
         size_t   inPitchBytesArr[kMaxNumInputs] = {0};
         uint32_t strideInElemArr[kMaxNumInputs] = {0};
         for (size_t i = 0; i < numInputs; i++) {
            inChannelsArr[i]   = static_cast<uint32_t>(currPrm.inChannels[i]);
            size_t widthInI    = currPrm.isInterleave ? inChannelsArr[i] : inSamples;
            inPitchBytesArr[i] = widthInI * dataSize;
            strideInElemArr[i] = static_cast<uint32_t>(widthInI);
         }

         // Output dimensions (output holds the total channels; tightly packed)
         size_t width            = currPrm.isInterleave ? totalInChannels : inSamples;
         size_t height           = currPrm.isInterleave ? inSamples : totalInChannels;
         size_t strideOut        = width * dataSize;
         size_t strideOutElements = width;
         size_t outputSize       = height * strideOut;

         // Allocate independent input buffers, each sized by its own channel count
         std::vector<decltype(myL2Heap.allocate64ByteAligned<uint8_t>(0))> inputBuffers;
         for (size_t i = 0; i < numInputs; i++) {
            size_t heightInI = currPrm.isInterleave ? inSamples : inChannelsArr[i];
            inputBuffers.push_back(myL2Heap.allocate64ByteAligned<uint8_t>(heightInI * inPitchBytesArr[i]));
         }

         auto  pOut   = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);
         void *pOutCn = (void *) ddrBuffer;
         memset(pOutCn, 0, outputSize);

         // Build void* array for reference C function
         void *inputArrayVoid[kMaxNumInputs] = {nullptr};
         for (size_t i = 0; i < numInputs; i++) {
            inputArrayVoid[i] = inputBuffers[i].get();
         }

         // Build inputs descriptor array
         std::vector<TISP::TISP_BufDesc2D> concatInputs(numInputs);
         for (size_t i = 0; i < numInputs; i++) {
            concatInputs[i].pBuf     = inputBuffers[i].get();
            concatInputs[i].width    = currPrm.isInterleave ? inChannelsArr[i] : inSamples;
            concatInputs[i].height   = currPrm.isInterleave ? inSamples : inChannelsArr[i];
            concatInputs[i].stride_y = static_cast<int32_t>(inPitchBytesArr[i]);
         }

         /* Only run the test if the buffer allocations fit in the heap */
         if (inputArrayVoid && pOut.get() && pOutCn) {

            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               for (size_t i = 0; i < numInputs; i++) {
                  size_t widthInI  = currPrm.isInterleave ? inChannelsArr[i] : inSamples;
                  size_t heightInI = currPrm.isInterleave ? inSamples : inChannelsArr[i];
                  TI_fillBuffer_float(currPrm.testPattern, 0, inputArrayVoid[i], currPrm.staticIn[i], widthInI,
                                      heightInI, inPitchBytesArr[i], AUDIOLIB_sizeof(currPrm.dataType),
                                      testPatternString);
               }
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_concat_createKernelObject(
                concatInputs.data(), pOut.get(), static_cast<uint32_t>(numInputs),
                static_cast<uint32_t>(inSamples), static_cast<uint8_t>(currPrm.isInterleave),
                strideOut, currPrm.dataType, errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            TI_profile_stop();

            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;
            kAddrMode = TISP::kAddrModeInOut;

            for (uint32_t i = 0; i < numInputs; i++) {
               // set address for block to process
               kAddr.src[i] = (uint8_t *) concatInputs[i].pBuf;
            }
            kAddr.dst[0] = (uint8_t *) pOut.get();
            k0->setAddr(kAddr, kAddrMode);

            TI_profile_start(TI_PROFILE_KERNEL_OPT);

            // Execute the kernel
            k0->exec();

            TI_profile_stop();

            for (size_t i = 0; i < 4; i++) {
               TISP::SetAddr_t     kAddr;
               TISP::SetAddrMode_t kAddrMode;

               kAddrMode = TISP::kAddrModeInOut;

               for (uint32_t i = 0; i < numInputs; i++) {
                  // set address for block to process
                  kAddr.src[i] = (uint8_t *) concatInputs[i].pBuf;
               }
               kAddr.dst[0] = (uint8_t *) pOut.get();
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

               // Execute the kernel
               k0->exec();
               TI_profile_stop();
            }

            for (size_t i = 0; i < numInputs; i++) {
               kAddr.src[i] = (uint8_t *) concatInputs[i].pBuf;
            }
            kAddr.dst[0] = (uint8_t *) pOut.get();
            k0->setAddr(kAddr, TISP::kAddrModeInOut);

            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);
            k0->exec();
            TI_profile_stop();

            // Run reference C implementation
            AUDIOLIB_concat_cn(inputArrayVoid, pOutCn, currPrm.dataType, static_cast<uint32_t>(numInputs),
                               currPrm.inChannels, static_cast<uint32_t>(inSamples), strideInElemArr,
                               static_cast<uint32_t>(strideOutElements), currPrm.isInterleave);

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               status_nat_vs_opt &= TI_compare_mem_2D_float(pOut.get(), pOutCn, 0, (double) powf(2, -10), width, height,
                                                            strideOut, dataSize);
            }
            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                  status_ref_vs_opt &=
                      TI_compare_mem_2DDualStride_float(pOut.get(), currPrm.staticOut, 0, (double) powf(2, -10), width,
                                                        height, strideOut, width * dataSize, dataSize);
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

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString, AUDIOLIB_sizeof(currPrm.dataType));

            // write to CSV, must happen prior to write to screen because
            fprintf(fpOutputCSV,
                    "TISP_concat, %d, %d,  "
                    "%d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.inSamples, currPrm.totalInChannels, archCycles,
                    pProfile[3 * tpi + 1], ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles),
                    !currentTestFail);

            TI_profile_add_test(testNum++, inSamples * totalInChannels, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, inSamples=%d, totalInChannels=%d", AUDIOLIB_sizeof(currPrm.dataType),
                    currPrm.inSamples, currPrm.totalInChannels);
            TI_profile_skip_test(desc);
            // clear the counters between runs; normally handled by
            // TI_profile_add_test
            TI_profile_clear_run_stats();
         } // end of memory allocation successful?
      } // end repetitions
   } // end idat test cases

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("_HOST_BUILD is defined.\n");
#endif
   return TISP_concat_d(&pProfile[0], 0);
}

/* Main call for individual test projects */
#ifdef __cplusplus
extern "C" {
#endif
int32_t test_main_d()
{
   int32_t fail = 1;

   uint32_t profile[256 * 3];

   /* TISP_TEST_init(); */

   fail = test_main(&profile[0]);

#if !defined(NO_PRINTF)
   if (fail == 0)
      printf("Test Pass!\n");
   else
      printf("Test Fail!\n");

   int32_t i;
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
