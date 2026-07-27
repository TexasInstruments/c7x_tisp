// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_cascadeBiquad_FIR_memory_map.hpp"
#include "TISP_profiler.hpp"
#include "TISP_rfft1dBatched.hpp"
#include "TISP_router.hpp"
#include "TISP_typeConversion.hpp"
#include "test_data/staticCase5.hpp"
#include <TISP_ErrorCtxt.hpp>
#include <TISP_test.h>
#include <TISP_types.hpp>
#include <cstddef>
#include <cstdint>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tisp.hpp>
#include <vector>

#define TISP_NUM_REPS (1)

#define DSPLIB_ADDCONSTANT_CYCLES (SIZE / 8)
#define DSPLIB_SQR_CYCLES (SIZE / 8)
#define ESITMATED_TOTAL_CYCLE_COUNT ((DSPLIB_ADDCONSTANT_CYCLES) + DSPLIB_SQR_CYCLES)
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

//! [create_graph]
void *create_graph()
{
   TISP::ErrorCtxt errorCtx;
   int32_t        *pIn         = (int32_t *) INPUT_ADDR;
   float          *pIntToFloat = INT_TO_FLOAT_ADDR;
   float          *pFIRCoeff   = FIR_COEFF_ADDR;
   float          *pFIRState   = FIR_STATE_ADDR;
   float          *pFIROut     = FIR_OUT_ADDR;

   float *pMatTransOut0 = MAT_TRANS_OUT0_ADDR;

   float *pCascade0Coeff     = CASCADE0_COEFF_ADDR;
   float *pCascade0FilterVar = CASCADE0_FILTER_VAR_ADDR;
   float *pCascade0Out       = CASCADE0_OUT_ADDR;

   float *pCascade1Coeff     = CASCADE1_COEFF_ADDR;
   float *pCascade1FilterVar = CASCADE1_FILTER_VAR_ADDR;
   float *pCascade1Out       = CASCADE1_OUT_ADDR;

   float *pMatTransOut1 = MAT_TRANS_OUT1_ADDR;

   float *pFFT_temp = FFT_TEMP_ADDR;
   float *pOutFFT   = FFT_OUT_ADDR;

   float *pIFFT_temp = IFFT_TEMP_ADDR;
   float *pOutIFFT   = IFFT_OUT_ADDR;

   float *pOutRouter = ROUTER_OUT_ADDR;

   int32_t *pConfigRouter  = (int32_t *) ROUTER_CONFIG_ADDR;
   float   *pScratchRouter = ROUTER_SCRATCH_ADDR;

   int32_t *pFloatToInt = (int32_t *) FLOAT_TO_INT_ADDR;

   // pConfigRouter = {0,1,2,3,4,5,6,7,8,0,1,2,3}
   for (auto i = 0; i < NUM_CHANNELS_OUT; i++) {
      if (i >= 8) {
         pConfigRouter[i] = i - 8;
      }
      else {

         pConfigRouter[i] = i;
      }
   }

   for (auto i = 0; i < BLOCK_SIZE * NUM_CHANNELS; i++) {
      pIn[i] = inputSignalCase5[i];
      /* pIn[i] = i; */
   }

   for (auto i = 0; i < (BLOCK_SIZE + NUM_FIR_TAPS - 1) * NUM_CHANNELS; i++) {
      pFIRState[i] = 0;
   }

   for (auto i = 0; i < NUM_FIR_TAPS; i++) {
      pFIRCoeff[i] = filterCoeffCase5[i];
   }

   for (auto i = 0; i < CASCADE_FILTER_SIZE; i++) {
      pCascade0Coeff[i] = cascadeBiquad0CoeffCase5[i];
      pCascade1Coeff[i] = cascadeBiquad1CoeffCase5[i];
   }

   for (auto i = 0; i < CASCADE_FILTER_VAR_SIZE; i++) {
      pCascade0FilterVar[i] = 0;
      pCascade1FilterVar[i] = 0;
   }

   printf("\n-------------------------------------------------");
   printf("\n  Audio Filtering  Demo using TISP");
   printf("\n-------------------------------------------------\n");

   printf("\nCreating Graph:\n");
   /* printf("\nL2-->DSPLIB_FIR-->L2\n"); */
   printf("\nL2-->DSPLIB_matTrans->DSPLIB_cascadeBiquad->DSPLIB_cascadeBiquad->DSPLIB_matTrans->DSPLIB_FIR->FFTLIB_"
          "RFFT->FFTLIB_IFFTR-->L2\n");

   /* streaming_engine::set_debug_level(1); */
   /* sa_generator::set_debug_level(1); */

   void *myGraph = nullptr;
   auto  myOpVec = new TISP::opVec();

   if (errorCtx.isSuccess()) {
      auto k0 = TISP::make_unique_node<TISP::AUDIOLIB::TypeConversion<int32_t, float>>(
          (int32_t *) pIn, (float *) pIntToFloat, NUM_CHANNELS, BLOCK_SIZE, NUM_CHANNELS * sizeof(int32_t),
          NUM_CHANNELS * sizeof(float), false, "TyperConversion: IntToFloat", 0, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k0));
      }
      else {
         printf("Error: Failed to create Node 0: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k1 = TISP::make_unique_node<TISP::DSPLIB::CascadeBiquad<float>>(
          (float *) pIntToFloat, (float *) pCascade0Coeff, (float *) pCascade0FilterVar, (float *) pCascade0Out,
          BLOCK_SIZE, NUM_CHANNELS, CASCADE_NUM_STAGES, DSPLIB_BIQUAD_DF1, (NUM_CHANNELS) * sizeof(float),
          (NUM_CHANNELS) * sizeof(float), NUM_CHANNELS * sizeof(float), false, "CascadeBiquad0", 1, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k1));
      }
      else {
         printf("Error: Failed to create Node 1: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k2 = TISP::make_unique_node<TISP::DSPLIB::CascadeBiquad<float>>(
          (float *) pCascade0Out, (float *) pCascade1Coeff, (float *) pCascade1FilterVar, (float *) pCascade1Out,
          BLOCK_SIZE, NUM_CHANNELS, CASCADE_NUM_STAGES, DSPLIB_BIQUAD_DF1, (NUM_CHANNELS) * sizeof(float),
          (NUM_CHANNELS) * sizeof(float), NUM_CHANNELS * sizeof(float), false, "CascadeBiquad1", 2, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k2));
      }
      else {
         printf("Error: Failed to create Node 2: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k3 = TISP::make_unique_node<TISP::DSPLIB::MatTrans<float>>(
          pCascade1Out, pMatTransOut1, NUM_CHANNELS, BLOCK_SIZE, NUM_CHANNELS * sizeof(float),
          BLOCK_SIZE * sizeof(float), "MatTrans0", 3, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k3));
      }
      else {
         printf("Error: Failed to create Node 3: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k4 = TISP::make_unique_node<TISP::DSPLIB::Fir<float>>(
          (float *) pMatTransOut1, (float *) pFIRState, (float *) pFIRCoeff, (float *) pFIROut, BLOCK_SIZE,
          NUM_FIR_TAPS, (BLOCK_SIZE + NUM_FIR_TAPS - 1), NUM_CHANNELS, BLOCK_SIZE * sizeof(float),
          (BLOCK_SIZE + NUM_FIR_TAPS - 1) * sizeof(float), NUM_FIR_TAPS * sizeof(float), BLOCK_SIZE * sizeof(float),
          false, false, false, 0, "FIR", 4, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k4));
      }
      else {
         printf("Error: Failed to create Node 4: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k5 = TISP::make_unique_node<TISP::FFTLIB::RFFT1dBatched<float>>(pFIROut, pFFT_temp, pOutFFT, BLOCK_SIZE,
                                                                           NUM_CHANNELS, 1, "RFFT", 5, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k5));
      }
      else {
         printf("Error: Failed to create Node 5: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k6 = TISP::make_unique_node<TISP::FFTLIB::IFFTR1dBatched<float>>(pOutFFT, pIFFT_temp, pOutIFFT, BLOCK_SIZE,
                                                                            NUM_CHANNELS, 1, "IFFTR", 6, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k6));
      }
      else {
         printf("Error: Failed to create Node 6: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k7 = TISP::make_unique_node<TISP::DSPLIB::MatTrans<float>>(
          pOutIFFT, pMatTransOut0, BLOCK_SIZE, NUM_CHANNELS, BLOCK_SIZE * sizeof(float), NUM_CHANNELS * sizeof(float),
          "MatTrans1", 7, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k7));
      }
      else {
         printf("Error: Failed to create Node 7: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k8 = TISP::make_unique_node<TISP::AUDIOLIB::Router<float>>(
          pMatTransOut0, (float *) pConfigRouter, (float *) pScratchRouter, (float *) pOutRouter, NUM_CHANNELS,
          BLOCK_SIZE, NUM_CHANNELS_OUT, NUM_CHANNELS * sizeof(float), NUM_CHANNELS_OUT * sizeof(float), true, "Router",
          8, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k8));
      }
      else {
         printf("Error: Failed to create Node 8: %s\n", errorCtx.getMessage());
      }
   }

   if (errorCtx.isSuccess()) {
      auto k9 = TISP::make_unique_node<TISP::AUDIOLIB::TypeConversion<float, int32_t>>(
          pOutRouter, pFloatToInt, NUM_CHANNELS_OUT, BLOCK_SIZE, (NUM_CHANNELS_OUT * sizeof(float)),
          (NUM_CHANNELS_OUT * sizeof(int32_t)), false, "TyperConversion: FloatToInt", 9, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k9));
      }
      else {
         printf("Error: Failed to create Node 9: %s\n", errorCtx.getMessage());
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
//! [create_graph]

//! [execute_graph]
void execute_graph(void *myGraph)
{

   uint32_t *pIn  = (uint32_t *) INPUT_ADDR;
   uint32_t *pOut = (uint32_t *) FLOAT_TO_INT_ADDR;

   TISP::opVec *myOpVec = static_cast<TISP::opVec *>(myGraph);
   size_t       length  = myOpVec->size();

   TISP::SetAddr_t addr;

   for (auto k = 0; k < (NUM_SAMPLES) / (BLOCK_SIZE); k++) {

      // begin poor man's DMA: memcopy a block
      for (auto j = 0; j < BLOCK_SIZE; j++) {
         for (auto i = 0; i < NUM_CHANNELS; i++) {
            pIn[j * NUM_CHANNELS + i] = inputSignalCase5[i + j * NUM_CHANNELS + k * BLOCK_SIZE * NUM_CHANNELS];
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
   NodeProfiler::writeJsonResults("TISP_cascadeBiquad_FIR.json", *myOpVec);

   /* for (auto i = 0; i < myOpVec->size(); i++) { */
   /*    printf("\n%d: %s\n", (*myOpVec)[i]->getNodeId(), (*myOpVec)[i]->getNodeName()); */
   /* } */
}
//! [execute_graph]

void check_output()
{

   /* float         *pIn     = INPUT_ADDR; */
   int32_t       *pOut    = (int32_t *) FLOAT_TO_INT_ADDR;
   const int32_t *pOutRef = outputSignalCase5;

   /* float         *pOutRouter = ROUTER_OUT_ADDR; */

   /* for (auto i = 0; i < BLOCK_SIZE; i++) { */
   /*    printf("\n%d: ", i); */
   /*    for (auto j = 0; j < NUM_CHANNELS_OUT; j++) { */
   /*       printf("%.2f, ", pOutRouter[i * NUM_CHANNELS_OUT + j]); */
   /*    } */
   /* } */

   /* const int32_t *pOutRef = inputSignalCase5; */
   /* const int32_t *pOutRef = (int32_t *) INPUT_ADDR; */

   /* int32_t status = TI_compare_mem_2D_float((void *) pOut, (void *) pOutRef, 0, (double) 1e-2, NUM_SAMPLES, */
   /*                                          NUM_CHANNELS, (NUM_SAMPLES) * sizeof(float), sizeof(DSPLIB_F32)); */

   // setting absolute tolerance of 2 for comparison
   int32_t status = TI_compare_mem_2D((void *) pOut, (void *) pOutRef, 2, (double) powf(2, -2), NUM_CHANNELS_OUT,
                                      NUM_SAMPLES, (NUM_CHANNELS_OUT) * sizeof(int32_t), sizeof(int32_t));

   if (status == TI_TEST_KERNEL_PASS) {
      printf("Opt and Ref match!  PASSED! \n");
   }
   else {
      printf("Opt and Ref DON'T match!  FAILED!!! \n");
   }

   printf("\n\n");

   /* for (auto i = 0; i < BLOCK_SIZE * NUM_CHANNELS; i++) { */
   /*    printf("%d ?= %d ", pOut[i], outputSignalCase3[i]); */
   /* } */

   printf("\n\n");

#if defined(HOST_EMULATION)

   // takes few minutes if enabled for EVM via xds110
   print_to_file("input.txt", inputSignalCase5);
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
