// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include AUDIOLIB
#include <audiolib.h>
#include <stddef.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <cstring>
#include <tisp.hpp>

// include test data for this kernel
#include "TISP_nlms_cn.h"
#include "TISP_nlms_idat.h"
#include "common/AUDIOLIB_bufParams.h"

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) // 2MB KB
#endif

#if defined(HOST_EMULATION)
#if defined(__C7504__)
int8_t l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t ddrBuffer[2048 * 1024];
int8_t l2SramHeap[L2SRAM_HEAP_SIZE];
#elif defined(__C7524__)
int8_t ddrBuffer[1 * 1024 * 1024];
int8_t l2SramHeap[L2SRAM_HEAP_SIZE];
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

std::unique_ptr<TISP::VirtualOp> TISP_nlms_createKernelObject(void            *pIn0,
                                                              void            *pIn1,
                                                              void            *pIn2,
                                                              void            *pIn3,
                                                              void            *pIn4,
                                                              void            *pOut,
                                                              void            *pInD,
                                                              void            *pInDesiredD,
                                                              void            *pOutI,
                                                              int32_t          width,
                                                              int32_t          height,
                                                              int32_t          widthS,
                                                              int32_t          heightS,
                                                              int32_t          filterLength,
                                                              float            stepSize,
                                                              int32_t          strideIn,
                                                              int32_t          strideOut,
                                                              uint32_t         sampleDataType,
                                                              uint32_t         isInterleave,
                                                              TISP::ErrorCtxt &errorCtx)
{
   std::unique_ptr<TISP::VirtualOp> kernel = nullptr;

   if (sampleDataType == AUDIOLIB_FLOAT32) {
      kernel = std::make_unique<TISP::AUDIOLIB::Nlms<float>>(
          (float *) pIn0, (float *) pIn1, (float *) pIn2, (float *) pIn3, (float *) pIn4, (float *) pOut,
          (float *) pInD, (float *) pInDesiredD, (float *) pOutI, width, height, widthS, heightS, filterLength,
          stepSize, strideIn, strideOut, isInterleave, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_nlms_createKernelObject", "unsupported data type for Nlms kernel");
   }
   return kernel;
}

int32_t readIdx, writeIdx;

int TISP_nlms_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail     = 0;
   int32_t  eleCount = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;
   uint64_t estCycles  = 0;

   TISP_nlms_testParams_t *prm;
   TISP_nlms_testParams_t  currPrm;
   TISP_nlms_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_nlms");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_nlms.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "WidthIn, HeightIn, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

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

         size_t dataSize     = AUDIOLIB_sizeof(currPrm.dataType);
         size_t filterLength = currPrm.filterLength;
         float  stepSize     = currPrm.stepSize;
         size_t numChannels  = currPrm.inChannels;
         size_t widthIn, heightIn, strideIn, widthOut, heightOut, strideOut, totalSamples;
         size_t numBlocks      = currPrm.numBlocks;
         size_t isInterleave   = currPrm.isInterleave;
         float  regularization = 0.001;

         if (isInterleave == 0) {
            widthIn  = currPrm.inSamples;
            heightIn = currPrm.inChannels;
            strideIn = currPrm.strideIn;

            widthOut     = currPrm.inSamples;
            heightOut    = currPrm.inChannels;
            strideOut    = currPrm.strideOut;
            totalSamples = widthIn;
         }
         else {
            widthIn  = currPrm.inChannels;
            heightIn = currPrm.inSamples;
            strideIn = currPrm.strideIn;

            widthOut     = currPrm.inChannels;
            heightOut    = currPrm.inSamples;
            strideOut    = currPrm.strideOut;
            totalSamples = heightIn;
         }

         size_t inpSizeBytes, outSizeBytes;

         inpSizeBytes = heightIn * strideIn;
         outSizeBytes = heightOut * strideOut;

         // --- State Buffer Allocation (Circular Buffer) ---
         // Calculate Power-of-2 Stride for Circular Addressing
         uint32_t CircularBufBaseMmemSize = 512;
         while (CircularBufBaseMmemSize < (currPrm.filterLength + currPrm.inSamples)) {
            CircularBufBaseMmemSize *= 2;
         }

         // This stride is used for alignment and memory allocation
         uint32_t stateStrideBytes = CircularBufBaseMmemSize * AUDIOLIB_sizeof(currPrm.dataType);
         uint32_t stateBufferSize  = currPrm.inChannels * stateStrideBytes;

         /* State matrix geometry handed to the node: the row length is the power-of-2 circular
          * buffer length (which the kernel also uses as the row stride) and there is one row per
          * channel. This is the extent of stateBufferSize, excluding the alignment slack below. */
         size_t widthS  = CircularBufBaseMmemSize;
         size_t heightS = numChannels;

         // --- Scratch Buffer Allocation (Accumulators) ---
         // Scratch size for accumulators (e.g., 1 filter length per channel)
         uint32_t scratchBuffSizeBytes = 2 * inpSizeBytes * AUDIOLIB_sizeof(currPrm.dataType);

         uint32_t minRequired = filterLength + totalSamples;

         uint32_t circBuffSize = 512;
         while (circBuffSize < minRequired) {
            circBuffSize *= 2;
         }
         circBuffSize          = circBuffSize;
         uint32_t circBuffMask = circBuffSize - 1;

         size_t strideStateElements = circBuffSize;
         size_t strideInElements    = strideIn / dataSize;
         size_t strideOutElements   = strideOut / dataSize;

         readIdx  = 0;
         writeIdx = filterLength;

         auto pIn        = myL2Heap.allocate64ByteAligned<uint8_t>(inpSizeBytes * numBlocks);
         auto pInDesired = myL2Heap.allocate64ByteAligned<uint8_t>(outSizeBytes * numBlocks);

         // Request extra space equal to the alignment minus 1 byte to safely shift forward
         auto pStateBufferAllocRaw   = myL2Heap.allocate64ByteAligned<uint8_t>(stateBufferSize + stateStrideBytes);
         auto pScratchBufferAllocRaw = myL2Heap.allocate64ByteAligned<uint8_t>(scratchBuffSizeBytes);
         auto pCoefficientsAllocRaw  = myL2Heap.allocate64ByteAligned<uint8_t>(scratchBuffSizeBytes);

         // Manually align the state buffer to stateStrideBytes boundary (power-of-2, >= 2048)
         // required for circular addressing. Scratch and coefficients only need 64-byte
         // alignment which allocate64ByteAligned already provides.
         size_t rawState     = reinterpret_cast<size_t>(pStateBufferAllocRaw.get());
         size_t alignedState = (rawState + stateStrideBytes - 1) & ~(static_cast<size_t>(stateStrideBytes) - 1);
         void  *pStateBuffer = reinterpret_cast<void *>(alignedState);

         void *pScratchBuffer = pScratchBufferAllocRaw.get();
         void *pCoefficients  = pCoefficientsAllocRaw.get();

         auto pOut = myL2Heap.allocate64ByteAligned<uint8_t>(outSizeBytes * numBlocks);

         void *pOutCn = (void *) ddrBuffer;
         memset(pOutCn, 0, (outSizeBytes * numBlocks));

         void *pInD        = (void *) (pOutCn + (outSizeBytes * numBlocks));
         void *pInDesiredD = (void *) (pInD + (inpSizeBytes * numBlocks));
         void *pOutI       = (void *) (pInDesiredD + (outSizeBytes * numBlocks));

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pInDesired.get() && pStateBuffer && pScratchBuffer && pCoefficients && pOut.get() && pOutCn) {
            if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
               for (size_t n = 0; n < numBlocks; n++) {
                  // fill input buffer
                  TI_fillBuffer_float(prm[tpi].testPattern, 0, (void *) ((int8_t *) pIn.get() + n * inpSizeBytes),
                                      (void *) ((float *) prm[tpi].staticIn0 + (n * widthIn * heightIn)), widthIn,
                                      heightIn, strideIn, AUDIOLIB_sizeof(currPrm.dataType), testPatternString);

                  TI_fillBuffer_float(prm[tpi].testPattern, 0,
                                      (void *) ((int8_t *) pInDesired.get() + n * inpSizeBytes),
                                      (void *) ((float *) prm[tpi].staticDesired + (n * widthIn * heightIn)), widthOut,
                                      heightOut, strideOut, AUDIOLIB_sizeof(currPrm.dataType), testPatternString);
               }
            }

            memset(pCoefficients, 0, scratchBuffSizeBytes);
            memset(pStateBuffer, 0, stateBufferSize);
            memset(pScratchBuffer, 0, scratchBuffSizeBytes); // Clear accumulators

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_nlms_createKernelObject(
                pIn.get(), pInDesired.get(), pStateBuffer, pScratchBuffer, pCoefficients, pOut.get(), pInD, pInDesiredD,
                pOutI, static_cast<uint32_t>(widthIn), static_cast<uint32_t>(heightIn), static_cast<uint32_t>(widthS),
                static_cast<uint32_t>(heightS), static_cast<uint32_t>(filterLength), static_cast<float>(stepSize),
                static_cast<uint32_t>(strideIn), static_cast<uint32_t>(strideOut), currPrm.dataType,
                static_cast<uint32_t>(isInterleave), errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            // initialize set function parameters
            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;

            //  set address for block to process
            kAddrMode    = TISP::kAddrModeInOut;
            kAddr.src[0] = (void *) ((int8_t *) pIn.get());
            kAddr.src[1] = (void *) ((int8_t *) pInDesired.get());
            kAddr.dst[0] = (void *) ((int8_t *) pOut.get());

            k0->setAddr(kAddr, kAddrMode);

            TI_profile_start(TI_PROFILE_KERNEL_OPT);

            // Execute the kernel
            k0->exec();

            TI_profile_stop();

            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */
            for (size_t n = 0; n < (numBlocks - 2); n++) {

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (void *) ((int8_t *) pIn.get() + (n + 1) * inpSizeBytes);
               kAddr.src[1] = (void *) ((int8_t *) pInDesired.get() + (n + 1) * outSizeBytes);
               kAddr.dst[0] = (void *) ((int8_t *) pOut.get() + (n + 1) * outSizeBytes);

               TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            // get output to L1D
            int16_t outSum   = 0;
            int8_t *pOutTemp = (int8_t *) pOut.get(); // treat output as bytes to be data type agnostic
            for (k = 0; k < widthIn; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            // dummy store of outSum to insure that the compiler does not remove it.
            volatileSum = outSum;

            if (numBlocks != 1) {

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (void *) ((int8_t *) pIn.get() + (numBlocks - 1) * inpSizeBytes);
               kAddr.src[1] = (void *) ((int8_t *) pInDesired.get() + (numBlocks - 1) * outSizeBytes);
               kAddr.dst[0] = (void *) ((int8_t *) pOut.get() + (numBlocks - 1) * outSizeBytes);
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            memset(pCoefficients, 0, scratchBuffSizeBytes);
            memset(pStateBuffer, 0, stateBufferSize);
            memset(pScratchBuffer, 0, scratchBuffSizeBytes); // Clear accumulators

            for (size_t n = 0; n < numBlocks; n++) {
               AUDIOLIB_nlms_cn((void *) ((int8_t *) pIn.get() + n * inpSizeBytes),
                                (void *) ((int8_t *) pInDesired.get() + n * outSizeBytes), pStateBuffer, pScratchBuffer,
                                pCoefficients, (void *) ((int8_t *) pOutCn + n * outSizeBytes), widthIn, heightIn,
                                totalSamples, filterLength, stepSize, circBuffMask, regularization, strideInElements,
                                strideOutElements, strideStateElements, currPrm.dataType, isInterleave);
            }
            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            for (size_t n = 0; n < numBlocks; n++) {
               if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                  status_nat_vs_opt &=
                      TI_compare_mem_2D_float((void *) ((int8_t *) pOut.get() + n * outSizeBytes),
                                              (void *) ((int8_t *) pOutCn + n * outSizeBytes), 0.001,
                                              (double) powf(2, -10), widthOut, heightOut, strideOut, dataSize);
               }
            }

            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                  status_ref_vs_opt = TI_TEST_KERNEL_PASS;
                  for (size_t n = 0; n < numBlocks; n++) {
                     status_ref_vs_opt &= TI_compare_mem_2DDualStride_float(
                         (void *) ((int8_t *) pOut.get() + n * outSizeBytes),
                         (void *) ((float *) currPrm.staticOut + n * widthIn * heightIn), 0.001, (double) powf(2, -10),
                         widthOut, heightOut, strideOut, widthOut * dataSize, dataSize);
                  }
               }
            }
            else {
               /* Set to pass since it wasn't supposed to run. */
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
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
                    "TISP_nlms, %d, %d, "
                    "%d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.inSamples, currPrm.inChannels, archCycles,
                    pProfile[3 * tpi + 1], ((float) cycles[TI_PROFILE_KERNEL_OPT_WARM]) / ((float) archCycles),
                    !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.inSamples * currPrm.inChannels, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", AUDIOLIB_sizeof(currPrm.dataType),
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
   return TISP_nlms_d(&pProfile[0], 0);
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
