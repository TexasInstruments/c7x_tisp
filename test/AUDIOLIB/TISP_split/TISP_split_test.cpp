// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include AUDIOLIB
#include "../common/TISP_test.h"
#include "../common/TISP_unit_test_l2_heap.hpp"
#include "../common/TI_profile.h"
#include "TISP_split.hpp"
#include "TISP_split_cn.h"
#include "TISP_split_idat.h"
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

std::unique_ptr<TISP::VirtualOp> TISP_split_createKernelObject(const TISP::TISP_BufDescList *pInputs,
                                                               const TISP::TISP_BufDescList *pOutputs,
                                                               uint32_t                      sampleDataType,
                                                               TISP::ErrorCtxt              &errorCtx)
{
   if (sampleDataType == AUDIOLIB_FLOAT32) {
      return std::make_unique<TISP::AUDIOLIB::Split<float>>(pInputs, pOutputs, "Split", 0, errorCtx);
   }
   else if (sampleDataType == AUDIOLIB_FLOAT64) {
      return std::make_unique<TISP::AUDIOLIB::Split<double>>(pInputs, pOutputs, "Split", 0, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_split_createKernelObject", "unsupported data type for Split kernel");
      return nullptr;
   }
}

#define AUDIOLIB_ROW_STRIDE(x, y) (((x + y) / y) * y)

int TISP_split_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
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

   TISP_split_testParams_t *prm;
   TISP_split_testParams_t  currPrm;
   TISP_split_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_split");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_split.csv", "w");
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

         constexpr size_t kMaxNumOutputs = static_cast<size_t>(TISP::kMaxDstAddr); // SetAddr_t dst[] capacity

         size_t dataSize           = AUDIOLIB_sizeof(currPrm.dataType);
         size_t inSamples          = currPrm.inSamples;
         size_t numOutputs         = currPrm.numOutputs;
         size_t totalInputChannels = currPrm.totalInputChannels;
         size_t isInterleave       = currPrm.isInputInterleave;

         // Input dimensions (holds the total channels; tightly packed)
         size_t widthIn          = isInterleave ? totalInputChannels : inSamples;
         size_t heightIn         = isInterleave ? inSamples : totalInputChannels;
         size_t strideIn         = widthIn * dataSize;
         size_t strideInElements = widthIn;
         size_t inputSize        = heightIn * strideIn;

         // Per-output channel counts + tight per-output pitches (bytes, elements) and sizes
         uint32_t outChannelsArr[kMaxNumOutputs]   = {0};
         size_t   outPitchBytesArr[kMaxNumOutputs] = {0};
         uint32_t strideOutElemArr[kMaxNumOutputs] = {0};
         size_t   outputSizeArr[kMaxNumOutputs]    = {0};
         for (size_t i = 0; i < numOutputs; i++) {
            outChannelsArr[i]   = currPrm.outChannels[i];
            size_t widthOutI    = isInterleave ? outChannelsArr[i] : inSamples;
            size_t heightOutI   = isInterleave ? inSamples : outChannelsArr[i];
            outPitchBytesArr[i] = widthOutI * dataSize;
            strideOutElemArr[i] = static_cast<uint32_t>(widthOutI);
            outputSizeArr[i]    = heightOutI * outPitchBytesArr[i];
         }

         // Allocate independent output buffers, each sized by its own channel count
         std::vector<decltype(myL2Heap.allocate64ByteAligned<uint8_t>(0))> outputBuffers;
         for (size_t i = 0; i < numOutputs; i++) {
            outputBuffers.push_back(myL2Heap.allocate64ByteAligned<uint8_t>(outputSizeArr[i]));
         }

         auto pIn = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);

         void **pOutCn = nullptr;
         if (currPrm.numOutputs > 0) {
            pOutCn = (void **) malloc(currPrm.numOutputs * sizeof(void *));
         }
         void    *pOutCnBase = (void *) ddrBuffer;
         uint32_t offset     = 0;
         for (size_t i = 0; i < currPrm.numOutputs; i++) {
            pOutCn[i] = (void *) (pOutCnBase + offset);
            offset += outputSizeArr[i];
         }

         // Build input and output descriptors
         const TISP::Types dataTypeTisp =
             (currPrm.dataType == AUDIOLIB_FLOAT64) ? TISP::Types::kFloat64 : TISP::Types::kFloat32;
         std::vector<TISP::TISP_BufDesc2D> splitOutputs(numOutputs);
         for (size_t i = 0; i < numOutputs; i++) {
            splitOutputs[i].pBuf         = outputBuffers[i].get();
            splitOutputs[i].dataType     = dataTypeTisp;
            splitOutputs[i].width        = isInterleave ? outChannelsArr[i] : inSamples;
            splitOutputs[i].height       = isInterleave ? inSamples : outChannelsArr[i];
            splitOutputs[i].pitchBytes   = outPitchBytesArr[i];
            splitOutputs[i].isInterleave = static_cast<uint8_t>(isInterleave);
         }
         const TISP::TISP_BufDesc2D   splitInput = {pIn.get(), dataTypeTisp, widthIn,
                                                    heightIn,  strideIn,     static_cast<uint8_t>(isInterleave)};
         const TISP::TISP_BufDescList inputList  = {&splitInput, 1U};
         const TISP::TISP_BufDescList outputList = {splitOutputs.data(), numOutputs};

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pOutCn) {

            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               TI_fillBuffer_float(currPrm.testPattern, 0, pIn.get(), currPrm.staticIn, widthIn, heightIn, strideIn,
                                   AUDIOLIB_sizeof(currPrm.dataType), testPatternString);
            }

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_split_createKernelObject(&inputList, &outputList, currPrm.dataType, errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }
            TI_profile_stop();

            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;
            kAddrMode = TISP::kAddrModeInOut;

            for (uint32_t i = 0; i < numOutputs; i++) {
               // set address for block to process
               kAddr.dst[i] = (uint8_t *) splitOutputs[i].pBuf;
            }
            kAddr.src[0] = (uint8_t *) pIn.get();
            k0->setAddr(kAddr, kAddrMode);

            TI_profile_start(TI_PROFILE_KERNEL_OPT);

            // Execute the kernel
            k0->exec();

            TI_profile_stop();

            for (size_t i = 0; i < 4; i++) {
               TISP::SetAddr_t     kAddr;
               TISP::SetAddrMode_t kAddrMode;
               kAddrMode = TISP::kAddrModeInOut;

               for (uint32_t i = 0; i < numOutputs; i++) {
                  // set address for block to process
                  kAddr.dst[i] = (uint8_t *) splitOutputs[i].pBuf;
               }
               kAddr.src[0] = (uint8_t *) pIn.get();
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

               // Execute the kernel
               k0->exec();
               TI_profile_stop();
            }

            for (uint32_t i = 0; i < numOutputs; i++) {
               // set address for block to process
               kAddr.dst[i] = (uint8_t *) splitOutputs[i].pBuf;
            }
            kAddr.src[0] = (uint8_t *) pIn.get();
            k0->setAddr(kAddr, kAddrMode);

            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);
            k0->exec();
            TI_profile_stop();

            // Run reference C implementation
            AUDIOLIB_split_cn(pIn.get(), pOutCn, currPrm.dataType, static_cast<uint32_t>(numOutputs),
                              static_cast<uint32_t>(inSamples), outChannelsArr, static_cast<uint32_t>(strideInElements),
                              strideOutElemArr, currPrm.isInputInterleave);

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            for (size_t i = 0; i < numOutputs; i++) {
               size_t widthOutI  = isInterleave ? outChannelsArr[i] : inSamples;
               size_t heightOutI = isInterleave ? inSamples : outChannelsArr[i];
               if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                  int32_t temp_status = TI_compare_mem_2DDualStride_float(
                      (void *) pOutCn[i], (void *) splitOutputs[i].pBuf, 0, (double) powf(2, -10), widthOutI, heightOutI,
                      outPitchBytesArr[i], widthOutI * AUDIOLIB_sizeof(currPrm.dataType),
                      AUDIOLIB_sizeof(currPrm.dataType));
                  if (temp_status == TI_TEST_KERNEL_FAIL) {
                     status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
                     break;
                  }
               }
            }
            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               for (size_t i = 0; i < numOutputs; i++) {
                  size_t widthOutI  = isInterleave ? outChannelsArr[i] : inSamples;
                  size_t heightOutI = isInterleave ? inSamples : outChannelsArr[i];
                  if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                     int32_t temp_status = TI_compare_mem_2DDualStride_float(
                         (void *) pOutCn[i], (void *) currPrm.staticOut[i], 0, (double) powf(2, -10), widthOutI,
                         heightOutI, outPitchBytesArr[i], widthOutI * AUDIOLIB_sizeof(currPrm.dataType),
                         AUDIOLIB_sizeof(currPrm.dataType));
                     if (temp_status == TI_TEST_KERNEL_FAIL) {
                        status_ref_vs_opt = TI_TEST_KERNEL_FAIL;
                        break;
                     }
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

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString, AUDIOLIB_sizeof(currPrm.dataType));

            // write to CSV, must happen prior to write to screen because
            fprintf(fpOutputCSV,
                    "TISP_split, %d, %d,  "
                    "%d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.inSamples, currPrm.totalInputChannels, archCycles,
                    pProfile[3 * tpi + 1], ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles),
                    !currentTestFail);

            TI_profile_add_test(testNum++, inSamples * totalInputChannels, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", AUDIOLIB_sizeof(currPrm.dataType),
                    currPrm.inSamples, currPrm.totalInputChannels);
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
   return TISP_split_d(&pProfile[0], 0);
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
