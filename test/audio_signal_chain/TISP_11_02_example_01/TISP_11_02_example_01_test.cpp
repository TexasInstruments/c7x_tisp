// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_11_02_example_01_memory_map.hpp"
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
   TISP::ErrorCtxt errorCtx;
   // Input and output pointers
   int32_t *pIn         = (int32_t *) INPUT_ADDR;
   float   *pIntToFloat = INT_TO_FLOAT_ADDR;

   // CascadeBiquad pointers
   float *pCascade0Coeff     = CASCADE0_COEFF_ADDR;
   float *pCascade0FilterVar = CASCADE0_FILTER_VAR_ADDR;
   float *pCascade0Out       = CASCADE0_OUT_ADDR;

   // GainNCh pointers
   float *pGainNChTarget = GAIN_NCH_TARGET_ADDR;
   float *pGainNChOut    = GAIN_NCH_OUT_ADDR;

   // MuteNCh pointers
   float   *pMuteNChInitialGain = MUTE_NCH_INITIAL_GAIN_ADDR;
   float   *pMuteNChTargetGain  = MUTE_NCH_TARGET_GAIN_ADDR;
   int32_t *pMuteNChIsMute      = (int32_t *) MUTE_NCH_IS_MUTE_ADDR;
   float   *pMuteNChOut         = MUTE_NCH_OUT_ADDR;

   // Final output pointer
   int32_t *pFloatToInt = (int32_t *) FLOAT_TO_INT_ADDR;

   // Copy input data
   for (auto i = 0; i < BLOCK_SIZE * NUM_CHANNELS; i++) {
      pIn[i] = inputSignalCase1[i];
   }

   // Copy cascade biquad coefficients
   for (auto i = 0; i < CASCADE_FILTER_SIZE; i++) {
      pCascade0Coeff[i] = cascadeBiquad0CoeffCase1[i];
   }

   // Initialize cascade biquad filter variables
   for (auto i = 0; i < CASCADE_FILTER_VAR_SIZE; i++) {
      pCascade0FilterVar[i] = 0;
   }

   // Copy gain values for gainNCh
   for (auto i = 0; i < NUM_CHANNELS; i++) {
      pGainNChTarget[i] = targetGain1[i];
   }

   // Copy gain values for muteNCh
   for (auto i = 0; i < NUM_CHANNELS; i++) {
      pMuteNChInitialGain[i] = initialGainMute1[i];
      pMuteNChTargetGain[i]  = targetGainMute1[i];
      pMuteNChIsMute[i]      = 0; // 0 = unmuted, 1 = muted
   }

   printf("\n-------------------------------------------------");
   printf("\n  11.2 Example Signal Chain 1 Demo using TISP");
   printf("\n-------------------------------------------------\n");

   printf("\nCreating Graph:\n");
   printf("\nL2-->AUDIOLIB_typeConversion->DSPLIB_cascadeBiquad->AUDIOLIB_gainNCh->AUDIOLIB_muteNCh->AUDIOLIB_"
          "typeConversion->L2\n");

   // Create the signal chain
   void *myGraph = nullptr;
   auto  myOpVec = new TISP::opVec();

   // 1. TypeConversion: int32_t to float
   if (errorCtx.isSuccess()) {
      auto k0 = TISP::make_unique_node<TISP::AUDIOLIB::TypeConversion<int32_t, float>>(
          (int32_t *) pIn, (float *) pIntToFloat, NUM_CHANNELS, BLOCK_SIZE, NUM_CHANNELS * sizeof(int32_t),
          NUM_CHANNELS * sizeof(float), 0, errorCtx);

      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k0));
      }
      else {
         printf("Error: Failed to create Node 1 (TypeConversion): %s\n", errorCtx.getMessage());
      }
   }

   // 2. CascadeBiquad: Apply parametric EQ
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

   // 3. GainNCh: Apply gain to each channel
   if (errorCtx.isSuccess()) {
      auto k2 = TISP::make_unique_node<TISP::AUDIOLIB::GainNCh<float>>(
          (float *) pCascade0Out, (float *) pGainNChTarget, (float *) pGainNChOut, NUM_CHANNELS, BLOCK_SIZE,
          NUM_CHANNELS, NUM_CHANNELS * sizeof(float), NUM_CHANNELS * sizeof(float), 1, errorCtx);

      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k2));
      }
      else {
         printf("Error: Failed to create Node 3 (GainNCh): %s\n", errorCtx.getMessage());
      }
   }

   // 4. MuteNCh: Apply mute/unmute to each channel
   if (errorCtx.isSuccess()) {
      auto k3 = TISP::make_unique_node<TISP::AUDIOLIB::Mutench<float>>(
          (float *) pGainNChOut, (float *) pMuteNChOut, NUM_CHANNELS, BLOCK_SIZE, NUM_CHANNELS,
          NUM_CHANNELS * sizeof(float), NUM_CHANNELS * sizeof(float), 1, 48000, (uint32_t *) pMuteNChIsMute, 0.0f, 2,
          errorCtx);

      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k3));
      }
      else {
         printf("Error: Failed to create Node 4 (MuteNCh): %s\n", errorCtx.getMessage());
      }
   }

   // 5. TypeConversion: float to int32_t
   if (errorCtx.isSuccess()) {
      auto k4 = TISP::make_unique_node<TISP::AUDIOLIB::TypeConversion<float, int32_t>>(
          (float *) pMuteNChOut, (int32_t *) pFloatToInt, NUM_CHANNELS, BLOCK_SIZE, NUM_CHANNELS * sizeof(float),
          NUM_CHANNELS * sizeof(int32_t), 0, errorCtx);

      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k4));
      }
      else {
         printf("Error: Failed to create Node 5 (TypeConversion): %s\n", errorCtx.getMessage());
      }
   }

   // Final evaluation and cleanup
   if (errorCtx.isSuccess()) {
      myGraph = myOpVec; // Assign to final return value if everything succeeded
   }
   else {
      delete myOpVec; // Destroy vector and properly free any nodes that DID succeed earlier
   }

   return myGraph;
}

void execute_graph(void *myGraph)
{
   int32_t *pIn  = (int32_t *) INPUT_ADDR;
   int32_t *pOut = (int32_t *) FLOAT_TO_INT_ADDR;

   TISP::opVec *myOpVec = static_cast<TISP::opVec *>(myGraph);
   size_t       length  = myOpVec->size();

   TISP::SetAddr_t addr;

   // Process data in blocks of BLOCK_SIZE
   for (auto k = 0; k < (NUM_SAMPLES) / (BLOCK_SIZE); k++) {
      // Copy input data for the current block
      for (auto j = 0; j < BLOCK_SIZE; j++) {
         for (auto i = 0; i < NUM_CHANNELS; i++) {
            pIn[j * NUM_CHANNELS + i] = inputSignalCase1[i + j * NUM_CHANNELS + k * BLOCK_SIZE * NUM_CHANNELS];
         }
      }

#if (!HOST_EMULATION)
      uint64_t startTime, endTime;
      startTime = (__get_GTSC(1), __STSC);
#endif

      // Set output address for the current block and execute graph
      addr.dst[0] = static_cast<void *>(pOut + (k) *NUM_CHANNELS_OUT * BLOCK_SIZE);
      (*myOpVec)[length - 1]->setAddr(addr, TISP::kAddrModeOut);
      TISP::execute::graph(myOpVec);

#if (!HOST_EMULATION)
      endTime            = (__get_GTSC(1), __STSC);
      uint64_t totalTime = (endTime - startTime) / (TISP_NUM_REPS);
      printf("Block %d: Total cycles to execute graph per frame = %llu\n", k, totalTime);
#endif
   }
}

void check_output()
{
   int32_t       *pOut    = (int32_t *) FLOAT_TO_INT_ADDR;
   const int32_t *pOutRef = outputSignalCase1;

   // Setting high absolute tolerance for comparison
   int32_t status = TI_compare_mem_2D((void *) pOut, (void *) pOutRef, 200, (double) powf(2, -2), NUM_CHANNELS_OUT,
                                      NUM_SAMPLES, (NUM_CHANNELS_OUT) * sizeof(int32_t), sizeof(int32_t));

   if (status == TI_TEST_KERNEL_PASS) {
      printf("Opt and Ref match!  PASSED! \n");
   }
   else {
      printf("Opt and Ref DON'T match!  FAILED!!! \n");
   }

#if defined(HOST_EMULATION)
   // Print output to file for debugging
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
