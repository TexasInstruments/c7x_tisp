// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <cstring>
#include <tisp.hpp>
#include <type_traits>

#include "TISP_node_factory.hpp"

#include "../../../src/FFTLIB/TISP_twiddle_lut.hpp"
#include "TISP_ifftr1dBatched_cn.h"
#include "TISP_ifftr1dBatched_idat.h"

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

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

static std::unique_ptr<TISP::VirtualOp> TISP_ifftr1dBatched_createKernelObject(void            *pIn,
                                                                               void            *pOut,
                                                                               void            *pScratch,
                                                                               size_t           scratchBytes,
                                                                               size_t           inWidth,
                                                                               size_t           outWidth,
                                                                               size_t           numChannels,
                                                                               bool             fullSpectrum,
                                                                               TISP::ErrorCtxt &errorCtx)
{
   const TISP::TISP_BufDesc2D inDesc  = {pIn, TISP::Types::kFloat32, inWidth, numChannels, inWidth * sizeof(float), 0U};
   const TISP::TISP_BufDesc2D outDesc = {pOut,        TISP::Types::kFloat32,    outWidth,
                                         numChannels, outWidth * sizeof(float), 0U};

   const TISP::TISP_BufDescList inputs  = {&inDesc, 1U};
   const TISP::TISP_BufDescList outputs = {&outDesc, 1U};

   TISP::FFTLIB::IFFTR1dBatchedConfig config;
   config.fullSpectrum = fullSpectrum;

   return std::make_unique<TISP::FFTLIB::IFFTR1dBatched<float>>(&inputs, &outputs, pScratch, scratchBytes, &config,
                                                                "IFFTR1dBatched", 0, errorCtx);
}

int TISP_ifftr1dBatched_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;

   TISP_ifftr1dBatched_testParams_t *prm;
   TISP_ifftr1dBatched_testParams_t  currPrm;
   TISP_ifftr1dBatched_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_ifftr1dBatched");

   // file IO for benchmarking
   FILE *fpOutputCSV = fopen("TISP_ifftr1dBatched.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "NumPoints, NumChannels, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

   // creates custom heap object
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
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

         currentTestFail = 0;

         const uint32_t numPoints    = currPrm.numPoints;
         const uint32_t numChannels  = currPrm.numChannels;
         const uint32_t fullSpectrum = currPrm.fullSpectrum;

         // complex bins per channel
         const size_t numBins  = fullSpectrum ? numPoints : ((numPoints / 2U) + 1U);
         const size_t inWidth  = 2U * numBins;
         const size_t outWidth = numPoints;

         // numBlocks independent frames are held back to back in the input and output buffers;
         // inputSize / outputSize are the sizes of ONE frame
         const size_t numBlocks   = currPrm.numBlocks;
         const size_t inputSize   = inWidth * numChannels * sizeof(float);
         const size_t outputSize  = outWidth * numChannels * sizeof(float);
         const size_t scratchSize = (size_t) numPoints * numChannels * sizeof(float);

         auto pIn      = myL2Heap.allocate64ByteAligned<uint8_t>(numBlocks * inputSize);
         auto pOut     = myL2Heap.allocate64ByteAligned<uint8_t>(numBlocks * outputSize);
         auto pScratch = myL2Heap.allocate64ByteAligned<uint8_t>(scratchSize);

         // natural C buffers live in DDR: output, scratch of the FFT, twiddles and split factors (and an input copy)
         float *pOutCn  = (float *) ddrBuffer;
         float *pXFFT   = pOutCn + outWidth * numChannels * numBlocks;
         float *pW      = pXFFT + (size_t) numPoints * numChannels;
         float *pSf     = pW + 4U * (size_t) numPoints;
         float *pInCn   = pSf + 2U * (size_t) numPoints;
         float *pInSave = pInCn + inWidth * numChannels;
         memset(pOutCn, 0, numBlocks * outputSize);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pOut.get() && pScratch.get()) {
            // fill input buffer, one frame at a time (static data holds the frames packed back to back)
            for (size_t n = 0; n < numBlocks; n++) {
               TI_fillBuffer_float(currPrm.testPattern, 0, pIn.get() + n * inputSize,
                                   (uint8_t *) currPrm.staticIn + n * inputSize, inWidth, numChannels,
                                   inWidth * sizeof(float), sizeof(float), testPatternString);
            }

            // the FFT kernels use their input as working memory: keep a copy of every frame to restore it
            // before every run
            memcpy(pInSave, pIn.get(), numBlocks * inputSize);

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;
            auto            k0 =
                TISP_ifftr1dBatched_createKernelObject(pIn.get(), pOut.get(), pScratch.get(), scratchSize, inWidth,
                                                       outWidth, numChannels, fullSpectrum != 0U, errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               fclose(fpOutputCSV);
               return -1;
            }

            TI_profile_stop();

            // initialize set function parameters
            TISP::SetAddr_t     kAddr;
            TISP::SetAddrMode_t kAddrMode;

            // every frame is executed in each timed phase, as in the other numBlocks tests. The FFT kernels
            // use their input as working memory, so each frame is restored from pInSave before it runs.
            for (size_t n = 0; n < numBlocks; n++) {
               memcpy(pIn.get() + n * inputSize, (uint8_t *) pInSave + n * inputSize, inputSize);

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * inputSize;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * outputSize;
               k0->setAddr(kAddr, kAddrMode);

               TI_profile_start(TI_PROFILE_KERNEL_OPT);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            // natural C reference, run on the (unmodified) input before the repeated runs below

            for (size_t n = 0; n < numBlocks; n++) {
               TISP_ifftr1dBatched_cn((float *) ((uint8_t *) pInSave + n * inputSize), pW, pSf, pXFFT,
                                      pOutCn + n * outWidth * numChannels, numPoints, numChannels,
                                      fullSpectrum != 0U);
            }

            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */
            for (k = 0; k < NUM_REPS; k++) {
               for (size_t n = 0; n < numBlocks; n++) {
                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);

                  memcpy(pIn.get() + n * inputSize, (uint8_t *) pInSave + n * inputSize, inputSize);

                  // set address for block to process
                  kAddrMode    = TISP::kAddrModeInOut;
                  kAddr.src[0] = (uint8_t *) pIn.get() + n * inputSize;
                  kAddr.dst[0] = (uint8_t *) pOut.get() + n * outputSize;
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
            for (k = 0; k < numBlocks * outputSize; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            // dummy store of outSum to insure that the compiler does not remove it.
            volatileSum = outSum;

            for (size_t n = 0; n < numBlocks; n++) {
               memcpy(pIn.get() + n * inputSize, (uint8_t *) pInSave + n * inputSize, inputSize);

               // set address for block to process
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get() + n * inputSize;
               kAddr.dst[0] = (uint8_t *) pOut.get() + n * outputSize;
               k0->setAddr(kAddr, kAddrMode);

               // run warm instruction cache test
               TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

               // Execute the kernel
               k0->exec();

               TI_profile_stop();
            }

            status_nat_vs_opt = TI_TEST_KERNEL_PASS;
            for (size_t n = 0; n < numBlocks; n++) {
               status_nat_vs_opt &= TI_compare_mem_2D_float(
                   (void *) (pOut.get() + n * outputSize), (void *) (pOutCn + n * outWidth * numChannels), 0,
                   (double) powf(2, -3), outWidth, numChannels, outWidth * sizeof(float), sizeof(float));
            }

            comparisonDone = 1;
            if (currPrm.staticOut != nullptr) {
               status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               for (size_t n = 0; n < numBlocks; n++) {
                  status_ref_vs_opt &= TI_compare_mem_2D_float(
                      (void *) (pOut.get() + n * outputSize), (void *) ((uint8_t *) currPrm.staticOut + n * outputSize),
                      0, (double) powf(2, -3), outWidth, numChannels, outWidth * sizeof(float), sizeof(float));
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

            sprintf(desc, "%s generated input | numPoints=%d numChannels=%d fullSpectrum=%d", testPatternString,
                    numPoints, numChannels, fullSpectrum);

            // write to CSV, must happen prior to write to screen because
            // TI_profile_formula_add clears values in counters
            fprintf(fpOutputCSV,
                    "TISP_ifftr1dBatched, %d, %d,  "
                    "%d, %d, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, numPoints, numChannels, archCycles, pProfile[3 * tpi + 1],
                    ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles), !currentTestFail);

            TI_profile_add_test(testNum++, numBlocks * numPoints * numChannels, 0, 0, currentTestFail, desc);
            TISP::handleAllocator().reset();
            TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "numPoints=%d, numChannels=%d does not fit the heap", numPoints, numChannels);
            TI_profile_skip_test(desc);
            // clear the counters between runs; normally handled by
            // TI_profile_add_test
            TI_profile_clear_run_stats();
         } // end of memory allocation successful?

         /* Reclaim the twiddle arena between test cases (the node above is destroyed): it is small and would
          * otherwise accumulate an entry for every FFT size */
         TISP::FFTLIB::TwiddleLUT<float>::reset();
      } // end repetitions
   } // end idat test cases

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile) { return TISP_ifftr1dBatched_d(&pProfile[0], 0); }

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
