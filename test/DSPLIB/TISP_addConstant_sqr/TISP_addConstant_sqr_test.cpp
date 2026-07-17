// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_addConstant_sqr_memory_map.hpp"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tisp.hpp>
#include <vector>

#define DSPLIB_ADDCONSTANT_CYCLES (BLOCK_SIZE / 8)
#define DSPLIB_SQR_CYCLES (BLOCK_SIZE / 8)
#define ESITMATED_TOTAL_CYCLE_COUNT ((DSPLIB_ADDCONSTANT_CYCLES) + DSPLIB_SQR_CYCLES)

/* #if !defined(HOST_EMULATION) */
/* #define printf DebugP_log */
/* #endif */

void TISP_addConstant_sqr_test_cn(float *pIn0, float *pIn1, float *pOut0, float *pOut1, size_t size);

#ifdef __cplusplus
extern "C" {
#endif
void *create_graph()
{

   NodeProfiler::initialize();

   float *pIn0 = INPUT0_ADDR;
   *pIn0       = 3.14;

   float *pIn1  = INPUT1_ADDR;
   float *pOut0 = OUTPUT0_ADDR;
   float *pOut1 = OUTPUT1_ADDR;

   float *pIn1Cn = INPUT1_CN_ADDR;

   for (uint32_t i = 0; i < (BLOCK_SIZE); i++) {
      pIn1[i]   = (float) rand() / (float) (RAND_MAX / (2 * 8)) - 8;
      pIn1Cn[i] = pIn1[i];
   }

   printf("\n-------------------------------------------------");
   printf("\n  DSPLIB_addConstant + DSPLIBsqr Demo using TISP");
   printf("\n-------------------------------------------------\n");

   printf("\nCreating Graph:\n");
   printf("\nL2-->DSPLIB_addConstant-->DSPLIB_sqr-->L2\n");

   TISP::ErrorCtxt errorCtx;
   auto            k0 = std::make_unique<TISP::DSPLIB::AddConstant<float>>(pIn0, pIn1, pOut0, BLOCK_SIZE, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto k1 = std::make_unique<TISP::DSPLIB::Sqr<float>>(pOut0, pOut1, BLOCK_SIZE, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto myOpVec = new TISP::opVec();

   myOpVec->push_back(std::move(k0));
   myOpVec->push_back(std::move(k1));

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

   for (size_t i = 0; i < (NUM_REPS); i++) {
      TISP::execute::graph(myOpVec);
   }

#if (!HOST_EMULATION)
   endTime            = (__get_GTSC(1), __STSC);
   uint64_t totalTime = (endTime - startTime) / (NUM_REPS);
   printf("Total cycles to execute graph = %llu\n", totalTime);
   printf("\nTheoritical cycle count (assuming flat memory w/ no stalls): %d\n", (ESITMATED_TOTAL_CYCLE_COUNT));
   printf("\nObeserved/Theoritical: %.2f\n", (((float) totalTime) / (ESITMATED_TOTAL_CYCLE_COUNT)));
#endif
   // Print the profiling results
   NodeProfiler::printResults();
}
void check_output()
{
   float *pIn0 = INPUT0_ADDR;

   float *pOut1 = OUTPUT1_ADDR;

   float *pIn1Cn  = INPUT1_CN_ADDR;
   float *pOut0Cn = OUTPUT0_CN_ADDR;
   float *pOut1Cn = OUTPUT1_CN_ADDR;

   TISP_addConstant_sqr_test_cn(pIn0, pIn1Cn, pOut0Cn, pOut1Cn, BLOCK_SIZE);

   int32_t status = TI_compare_mem_2D_float((void *) pOut1, (void *) pOut1Cn, 0, (double) powf(2, -2), BLOCK_SIZE, 1, 1,
                                            sizeof(float));

   if (status == TI_TEST_KERNEL_PASS) {
      printf("Opt and NatC match!  PASSED! \n");
   }
   else {
      printf("Opt and NatC DON'T match!  FAILED!!! \n");
   }

   return;
}

void delete_graph(void *myGraph)
{
   TISP::opVec *myOpVec = static_cast<TISP::opVec *>(myGraph);
   delete (myOpVec);
}

#ifdef __cplusplus
}
#endif
