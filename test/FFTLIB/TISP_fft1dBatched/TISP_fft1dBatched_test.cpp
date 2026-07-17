// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_fft1dBatched_memory_map.hpp"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tisp.hpp>
#include <vector>

#define MISC_BLOCK_COPY_CYCLES ((NUM_POINTS * NUM_CHANNELS) / 4)
#define FFTLIB_FFT1DBATCHED_32X512_CYCLES (25592)
#define DSPLIB_SQR_CYCLES ((NUM_POINTS * NUM_CHANNELS) / 4)
#define ESITMATED_TOTAL_CYCLE_COUNT ((MISC_BLOCK_COPY_CYCLES + FFTLIB_FFT1DBATCHED_32X512_CYCLES) + DSPLIB_SQR_CYCLES)

void TISP_1dBatched_test_cn(float *pX, float *pW, float *pY0, float *pY1, size_t numPoints, size_t numChannels);

#ifdef __cplusplus
extern "C" {
#endif
void *create_graph()
{

   float *pX0 = INPUT_BLOCK_ADDR;
   float *pY0 = COPY_BLOCK_ADDR;
   float *pY1 = FFT_BLOCK_ADDR;
   float *pY2 = SQR_BLOCK_ADDR;

   // No location for copying block in natC
   float *pXCn = INPUT_CN_ADDR;

   for (uint32_t i = 0; i < NUM_CHANNELS * NUM_POINTS * 2; i++) {
      pX0[i]  = (float) rand() / (float) (RAND_MAX / (2 * 8)) - 8;
      pXCn[i] = pX0[i];
   }

   printf("\n-------------------------------------------------");
   printf("\n        FFT 1D Batched + Sqr Demo using TISP     ");
   printf("\n-------------------------------------------------\n");

   printf("\nCreating Graph:\n");
   printf("\nL2-->blockCopy-->fft1dBatched-->DSPLIB_sqr-->L2\n");

   //! [connecting nodes example]

   TISP::ErrorCtxt errorCtx;
   auto k0 = std::make_unique<TISP::misc::BlockCopy<float>>(pX0, pY0, NUM_POINTS * 2, NUM_CHANNELS, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto k1 = std::make_unique<TISP::FFTLIB::FFT1dBatched<float>>(pY0, pY1, NUM_POINTS, NUM_CHANNELS, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto k2 = std::make_unique<TISP::DSPLIB::Sqr<float>>(pY1, pY2, 2 * NUM_POINTS * NUM_CHANNELS, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto myOpVec = new TISP::opVec();

   myOpVec->push_back(std::move(k0));
   myOpVec->push_back(std::move(k1));
   myOpVec->push_back(std::move(k2));
   //! [connecting nodes example]

   void *myGraph = myOpVec;

   return myGraph;
}

void execute_graph(void *myGraph)
{

#if (!HOST_EMULATION)
   uint64_t startTime, endTime;
   startTime = (__get_GTSC(1), __STSC);
#endif

   TISP::opVec *myOpVec = static_cast<TISP::opVec *>(myGraph);

   for (int32_t i = 0; i < (NUM_REPS); i++) {
      TISP::execute::graph(myOpVec);
   }

#if (!HOST_EMULATION)
   endTime            = (__get_GTSC(1), __STSC);
   uint64_t totalTime = (endTime - startTime) / (NUM_REPS);
   printf("Total cycles to execute graph = %llu\n", totalTime);
   printf("\nTheoritical cycle count (assuming flat memory w/ no stalls): %d\n", (ESITMATED_TOTAL_CYCLE_COUNT));
   printf("\nObeserved/Theoritical: %.2f\n", (((float) totalTime) / (ESITMATED_TOTAL_CYCLE_COUNT)));
#endif
}

void check_output()
{

   float *pXCn  = INPUT_CN_ADDR;
   float *pY0Cn = FFT_CN_ADDR;
   float *pY1Cn = SQR_CN_ADDR;
   float *pWCn  = TW_CN_ADDR;

   float *pY2 = SQR_BLOCK_ADDR;

   TISP_1dBatched_test_cn(pXCn, pWCn, pY0Cn, pY1Cn, NUM_POINTS, NUM_CHANNELS);

   int32_t status = TI_compare_mem_2D_float((void *) pY2, (void *) pY1Cn, 0, (double) powf(2, -2), NUM_POINTS << 1,
                                            NUM_CHANNELS, (NUM_POINTS << 1) * sizeof(float), sizeof(float));

   if (status == TI_TEST_KERNEL_PASS) {
      printf("Opt and NatC match!  PASSED! \n");
   }
   else {
      printf("Opt and NatC DON'T match!  FAILED!!! \n");
   }
}

void delete_graph(void *myGraph)
{
   TISP::opVec *myOpVec = static_cast<TISP::opVec *>(myGraph);
   delete (myOpVec);
}

#ifdef __cplusplus
}
#endif
