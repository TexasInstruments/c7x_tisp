// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_parametericEq_memory_map.hpp"
#include "test_data/staticCase1.hpp"
#include <TISP_ErrorCtxt.hpp>
#include <TISP_profile.hpp>
#include <TISP_test.h>
#include <TISP_types.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tisp.hpp>
#include <vector>

#define TISP_NUM_REPS (1)

#if defined(HOST_EMULATION)
template <typename T> void print_to_file(const char *fileName, const T *pData)
{
   FILE *fp = fopen(fileName, "w");
   if (fp == NULL) {
      printf("Error opening file for writing\n");
      return;
   }
   for (int i = 0; i < NUM_CHANNELS * NUM_SAMPLES; i++) {
      fprintf(fp, "%d\n", pData[i]);
   }

   fclose(fp);
}

template <> void print_to_file<float>(const char *fileName, const float *pData)
{

   FILE *fp = fopen(fileName, "w");
   if (fp == NULL) {
      printf("Error opening file for writing\n");
      return;
   }

   for (int i = 0; i < NUM_CHANNELS * NUM_SAMPLES; i++) {
      fprintf(fp, "%f\n", pData[i]);
   }

   fclose(fp);
}

#endif

#ifdef __cplusplus
extern "C" {
#endif

void *create_graph()
{

   int32_t *pIn         = (int32_t *) INPUT_ADDR;
   float   *pIntToFloat = INT_TO_FLOAT_ADDR;

   float *pCascade0Coeff     = CASCADE0_COEFF_ADDR;
   float *pCascade0FilterVar = CASCADE0_FILTER_VAR_ADDR;
   float *pCascade0Out       = CASCADE0_OUT_ADDR;

   /* float *pOutRouter = ROUTER_OUT_ADDR; */

   /* int32_t *pConfigRouter  = (int32_t *) ROUTER_CONFIG_ADDR; */
   /* float   *pScratchRouter = ROUTER_SCRATCH_ADDR; */

   int32_t *pFloatToInt = (int32_t *) FLOAT_TO_INT_ADDR;

   // pConfigRouter = {0,1,2,3,4,5,6,7,8,0,1,2,3}
   /* for (auto i = 0; i < NUM_CHANNELS_OUT; i++) { */
   /*    if (i >= 8) { */
   /*       pConfigRouter[i] = i - 8; */
   /*    } */
   /*    else { */

   /*       pConfigRouter[i] = i; */
   /*    } */
   /* } */

   for (auto i = 0; i < BLOCK_SIZE * NUM_CHANNELS; i++) {
      pIn[i] = inputSignalCase1[i];
   }

   for (auto i = 0; i < CASCADE_FILTER_SIZE; i++) {
      pCascade0Coeff[i] = cascadeBiquad0CoeffCase1[i];
   }

   for (auto i = 0; i < CASCADE_FILTER_VAR_SIZE; i++) {
      pCascade0FilterVar[i] = 0;
   }

   printf("\n-------------------------------------------------");
   printf("\n  Parameteric EQ  Demo using TISP");
   printf("\n-------------------------------------------------\n");

   printf("\nCreating Graph:\n");
   printf("\nL2-->AUDIOLIB_typeConversion->DSPLIB_cascadeBiquad->AUDIOLIB_typeConversion->L2\n");

   TISP::ErrorCtxt errorCtx;

   void *myGraph = nullptr;
   auto  myOpVec = new TISP::opVec();

   if (errorCtx.isSuccess()) {
      auto k0 = TISP::make_unique_node<TISP::AUDIOLIB::TypeConversion<int32_t, float>>(
          (int32_t *) pIn, (float *) pIntToFloat, NUM_CHANNELS, BLOCK_SIZE, NUM_CHANNELS * sizeof(int32_t),
          NUM_CHANNELS * sizeof(float), (size_t) 1, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k0));
      }
      else {
         printf("Error: Failed to create Node 1 (TypeConversion): %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k1 = TISP::make_unique_node<TISP::DSPLIB::CascadeBiquad<float>>(
          (float *) pIntToFloat, (float *) pCascade0Coeff, (float *) pCascade0FilterVar, (float *) pCascade0Out,
          BLOCK_SIZE, NUM_CHANNELS, CASCADE_NUM_STAGES, DSPLIB_BIQUAD_DF1, (NUM_CHANNELS) * sizeof(float),
          (NUM_CHANNELS) * sizeof(float), NUM_CHANNELS * sizeof(float), false, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k1));
      }
      else {
         printf("Error: Failed to create Node 2 (CascadeBiquad): %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k2 = TISP::make_unique_node<TISP::AUDIOLIB::TypeConversion<float, int32_t>>(
          (float *) pCascade0Out, (int32_t *) pFloatToInt, NUM_CHANNELS, BLOCK_SIZE, NUM_CHANNELS * sizeof(float),
          NUM_CHANNELS * sizeof(int32_t), (size_t) 1, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k2));
      }
      else {
         printf("Error: Failed to create Node 3 (TypeConversion): %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      myGraph = myOpVec;
      TISP::printArenaStats();
   }
   else {
      delete myOpVec;
   }

   return myGraph;
}

void execute_graph(void *myGraph)
{

   int32_t *pIn  = (int32_t *) INPUT_ADDR;
   int32_t *pOut = (int32_t *) FLOAT_TO_INT_ADDR;
   /* float *pOut = (float *) INT_TO_FLOAT_ADDR; */

   TISP::opVec *myOpVec = static_cast<TISP::opVec *>(myGraph);
   size_t       length  = myOpVec->size();

   TISP::SetAddr_t addr;

   for (auto k = 0; k < (NUM_SAMPLES) / (BLOCK_SIZE); k++) {

      // begin poor man's DMA: memcopy a block
      for (auto j = 0; j < BLOCK_SIZE; j++) {
         for (auto i = 0; i < NUM_CHANNELS; i++) {
            pIn[j * NUM_CHANNELS + i] = inputSignalCase1[i + j * NUM_CHANNELS + k * BLOCK_SIZE * NUM_CHANNELS];
         }
      }
      // end poor man's DMA
#if (!HOST_EMULATION)
      uint64_t startTime, endTime;
      startTime = (__get_GTSC(1), __STSC);
#endif

      addr.dst[0] = static_cast<void *>(pOut + (k) *NUM_CHANNELS_OUT * BLOCK_SIZE);
      (*myOpVec)[length - 1]->setAddr(addr, TISP::kAddrModeOut);
      TISP::execute::graph(myOpVec);

#if (!HOST_EMULATION)
      endTime            = (__get_GTSC(1), __STSC);
      uint64_t totalTime = (endTime - startTime) / (TISP_NUM_REPS);
      printf("Total cycles to execute graph per frame = %llu\n", totalTime);

#endif
   }
}

void check_output()
{

   int32_t       *pOut    = (int32_t *) FLOAT_TO_INT_ADDR;
   const int32_t *pOutRef = outputSignalCase1;

   /* float         *pOutRouter = ROUTER_OUT_ADDR; */

   // setting high absolute tolerance for comparison; TODO: investigate poor snr (around 95dB)
   int32_t status = TI_compare_mem_2D((void *) pOut, (void *) pOutRef, 200, (double) powf(2, -2), NUM_CHANNELS_OUT,
                                      NUM_SAMPLES, (NUM_CHANNELS_OUT) * sizeof(int32_t), sizeof(int32_t));

   if (status == TI_TEST_KERNEL_PASS) {
      printf("Opt and Ref match!  PASSED! \n");
   }
   else {
      printf("Opt and Ref DON'T match!  FAILED!!! \n");
   }

   // Calculate and print SNR values
   /* printf("Calculating SNR values...\n"); */
   /* TISP::print_snr(pOutRef, pOut, NUM_SAMPLES, NUM_CHANNELS_OUT, TISP::DataMode::kInterleave); */
   /* printf("Exitting test case!\n"); */

#if defined(HOST_EMULATION)

   // takes few minutes if enabled for EVM via xds110
   /* print_to_file("input.txt", inputSignalCase1); */
   print_to_file("output.txt", pOut);

   printf("Printing to file done!\n");

#endif

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
