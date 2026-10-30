// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include AUDIOLIB
#include <audiolib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <cstring>
#include <tisp.hpp>

// include test data for this kernel
#include "TISP_router_cn.h"
#include "TISP_router_idat.h"
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

template <typename T>
std::unique_ptr<TISP::VirtualOp> TISP_router_createNode(void            *pIn,
                                                        void            *pRouter,
                                                        void            *pScratch,
                                                        void            *pOut,
                                                        size_t           widthIn,
                                                        size_t           heightIn,
                                                        size_t           outChannels,
                                                        size_t           strideIn,
                                                        size_t           strideOut,
                                                        size_t           scratchBytes,
                                                        uint8_t          isInterleave,
                                                        TISP::ErrorCtxt &errorCtx)
{
   const size_t widthOut  = (isInterleave == 1U) ? outChannels : widthIn;
   const size_t heightOut = (isInterleave == 1U) ? heightIn : outChannels;

   const TISP::TISP_BufDesc2D inDesc  = {pIn, TISP::ElementDType<T>::value, widthIn, heightIn, strideIn, isInterleave};
   const TISP::TISP_BufDesc2D outDesc = {pOut,      TISP::ElementDType<T>::value, widthOut, heightOut, strideOut,
                                         isInterleave};

   const TISP::TISP_BufDescList inputs  = {&inDesc, 1U};
   const TISP::TISP_BufDescList outputs = {&outDesc, 1U};

   TISP::AUDIOLIB::RouterTunableParams<T> defaultParams;
   defaultParams.pChannelIndices = static_cast<uint32_t *>(pRouter);

   return std::make_unique<TISP::AUDIOLIB::Router<T>>(&inputs, &outputs, pScratch, scratchBytes, &defaultParams,
                                                      "Router", 0, errorCtx);
}

std::unique_ptr<TISP::VirtualOp> TISP_router_createKernelObject(void            *pIn,
                                                                void            *pRouter,
                                                                void            *pScratch,
                                                                void            *pOut,
                                                                size_t           widthIn,
                                                                size_t           heightIn,
                                                                size_t           outChannels,
                                                                size_t           strideIn,
                                                                size_t           strideOut,
                                                                size_t           scratchBytes,
                                                                uint8_t          isInterleave,
                                                                uint32_t         dataType,
                                                                TISP::ErrorCtxt &errorCtx)
{
   if (dataType == AUDIOLIB_FLOAT32) {
      return TISP_router_createNode<float>(pIn, pRouter, pScratch, pOut, widthIn, heightIn, outChannels, strideIn,
                                           strideOut, scratchBytes, isInterleave, errorCtx);
   }
   else if (dataType == AUDIOLIB_FLOAT64) {
      return TISP_router_createNode<double>(pIn, pRouter, pScratch, pOut, widthIn, heightIn, outChannels, strideIn,
                                            strideOut, scratchBytes, isInterleave, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_router_createKernelObject", "unsupported data type for Router kernel");
      return nullptr;
   }
}

int TISP_router_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
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

   TISP_router_testParams_t *prm;
   TISP_router_testParams_t  currPrm;
   TISP_router_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_router");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_router.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "WidthIn, HeightIn, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      size_t isInterleave = currPrm.isInterleave;
      size_t outChannels  = currPrm.outChannels;

      for (repCount = 0; repCount < numReps; repCount++) {
         int32_t status_nat_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparij * strideIn + ison is done. This flag is to detect
          * if a comparison was done or not                                   */
         int32_t comparisonDone = 0;
         currentTestFail        = 0;

         size_t dataSize  = AUDIOLIB_sizeof(currPrm.dataType);
         size_t strideIn  = currPrm.strideIn;
         size_t strideOut = currPrm.strideOut;
         size_t numBlocks = currPrm.numBlocks;
         size_t widthIn, heightIn, widthOut, heightOut;

         if (isInterleave == 1) {
            widthIn   = currPrm.heightIn;
            heightIn  = currPrm.widthIn;
            widthOut  = currPrm.outChannels;
            heightOut = currPrm.widthIn;
         }
         else {
            widthIn   = currPrm.widthIn;
            heightIn  = currPrm.heightIn;
            widthOut  = currPrm.widthIn;
            heightOut = currPrm.outChannels;
         }

         // Here, stride is in bytes

         if ((AUDIOLIB_sizeof(currPrm.dataType) == 4) || (AUDIOLIB_sizeof(currPrm.dataType) == 8)) {
            eleCount = 2 * (__C7X_VEC_SIZE_BYTES__ / AUDIOLIB_sizeof(currPrm.dataType));
         }
         else {
            eleCount = __C7X_VEC_SIZE_BYTES__ / AUDIOLIB_sizeof(currPrm.dataType);
         }

         int32_t dim_y_padded  = AUDIOLIB_ROW_STRIDE(heightIn, eleCount);
         int32_t dim_y_padded1 = AUDIOLIB_ROW_STRIDE(heightOut, eleCount);

         size_t inputSize, routerSize, outputSize, scratchBufSize;

         routerSize     = (outChannels * sizeof(uint32_t));
         outputSize     = (numBlocks * heightOut * strideOut);
         scratchBufSize = (dim_y_padded1 * currPrm.widthIn * dataSize);

         inputSize = (isInterleave == 1) ? (numBlocks * dim_y_padded * strideIn) : (numBlocks * heightIn * strideIn);

         size_t strideInElements  = (strideIn / dataSize);
         size_t strideOutElements = (strideOut / dataSize);
         size_t total_size        = (inputSize + routerSize + scratchBufSize + outputSize);

         auto pIn      = myL2Heap.allocate64ByteAligned<uint8_t>(inputSize);
         auto pRouter  = myL2Heap.allocate64ByteAligned<uint8_t>(routerSize);
         auto pScratch = myL2Heap.allocate64ByteAligned<uint8_t>(scratchBufSize);
         auto pOut     = myL2Heap.allocate64ByteAligned<uint8_t>(outputSize);

         void     *pOutCn            = (void *) ddrBuffer;
         int32_t  *muteArray         = (int32_t *) (ddrBuffer + outputSize);
         uint32_t *outputOffsetArray = (uint32_t *) (ddrBuffer + outputSize + routerSize);

         memset(pOutCn, 0, outputSize);
         memset(muteArray, 0, routerSize);
         memset(outputOffsetArray, 0, routerSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pRouter.get() && pScratch.get() && pOut.get() && pOutCn) {
            for (size_t n = 0; n < numBlocks; n++) {
               if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                  // fill input buffer
                  TI_fillBuffer_float(prm[tpi].testPattern, 0, pIn.get() + n * heightIn * strideIn,
                                      prm[tpi].staticIn0 + n * heightIn * widthIn * dataSize, widthIn, heightIn,
                                      strideIn, dataSize, testPatternString);
               }
            }

            memcpy(pRouter.get(), prm[tpi].staticIn1, routerSize);

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 = TISP_router_createKernelObject(
                pIn.get(), pRouter.get(), pScratch.get(), pOut.get(), widthIn, heightIn, outChannels, strideIn,
                strideOut, scratchBufSize, static_cast<uint8_t>(isInterleave), currPrm.dataType, errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            TI_profile_stop();

            // initialize set function parameters
            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;

            for (size_t n = 0; n < numBlocks; n++) {
               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightOut * strideOut;

               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */
            for (k = 0; k < NUM_REPS; k++) {

               for (size_t n = 0; n < numBlocks; n++) {

                  // set address for block to process
                  kAddrMode    = TISP::kAddrModeInOut;
                  kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;
                  kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightOut * strideOut;

                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

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
            for (k = 0; k < widthOut; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            // dummy store of outSum to insure that the compiler does not remove it.
            volatileSum = outSum;

            for (size_t n = 0; n < numBlocks; n++) {

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * heightIn * strideIn;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * heightOut * strideOut;
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }
            outputSize = widthIn;

            AUDIOLIB_router_cn_set(pRouter.get(), currPrm.heightIn, outChannels, strideInElements, isInterleave,
                                   muteArray, outputOffsetArray);

            for (size_t n = 0; n < numBlocks; n++) {
               AUDIOLIB_router_cn(pIn.get() + n * heightIn * strideIn, pOutCn + n * heightOut * strideOut,
                                  pScratch.get(), currPrm.dataType, widthOut, heightOut, strideInElements,
                                  strideOutElements, outChannels, muteArray, outputOffsetArray, isInterleave);
            }

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            for (size_t n = 0; n < numBlocks; n++) {
               status_nat_vs_opt &= TI_compare_mem_2D_float(
                   (void *) pOut.get() + n * heightOut * strideOut, (void *) pOutCn + n * heightOut * strideOut, 0,
                   (double) powf(2, -10), widthOut, heightOut, strideOut, dataSize);
            }

            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               for (size_t n = 0; n < numBlocks; n++) {
                  if (currPrm.dataType == AUDIOLIB_FLOAT32 || currPrm.dataType == AUDIOLIB_FLOAT64) {
                     status_ref_vs_opt &= TI_compare_mem_2DDualStride_float(
                         (void *) pOut.get() + n * heightOut * strideOut,
                         (void *) currPrm.staticOut + n * widthOut * heightOut * dataSize, 0, (double) powf(2, -10),
                         widthOut, heightOut, strideOut, widthOut * dataSize, dataSize);
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
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "TISP_router, %d, %d, "
                    "%d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, currPrm.widthIn, currPrm.heightIn, archCycles, pProfile[3 * tpi + 1],
                    ((float) cycles[TI_PROFILE_KERNEL_OPT_WARM]) / ((float) archCycles), !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.widthIn * currPrm.heightIn, 0, 0, currentTestFail, desc);
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
   return TISP_router_d(&pProfile[0], 0);
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
