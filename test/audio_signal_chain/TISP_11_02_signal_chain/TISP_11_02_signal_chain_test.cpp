// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_11_02_signal_chain_memory_map.hpp"
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
#define DEBUG (0)

#if defined(HOST_EMULATION)
template <typename T> void print_to_file(const char *fileName, const T *pData)
{
   FILE *fp = fopen(fileName, "w");
   if (fp == NULL) {
      printf("Error opening file for writing\n");
      return;
   }
   for (int i = 0; i < NUM_CHANNELS_OUT * NUM_SAMPLES; i++) {
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

   for (int i = 0; i < NUM_CHANNELS_OUT * NUM_SAMPLES; i++) {
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
   int32_t *pIn            = (int32_t *) INPUT_ADDR;
   float   *pTypeConvInOut = TYPE_CONV_IN_OUT_ADDR;

   // GainNCh pointers
   float *pGainNChTarget = GAIN_NCH_TARGET_ADDR;
   float *pGainNChOut    = GAIN_NCH_OUT_ADDR;

   // CascadeBiquad pointers
   float *pCascadeCoeff     = CASCADE_COEFF_ADDR;
   float *pCascadeFilterVar = CASCADE_FILTER_VAR_ADDR;
   float *pCascadeOut       = CASCADE_OUT_ADDR;

   // Router pointers
   int32_t *pRouterConfig  = (int32_t *) ROUTER_OUT_CHANNELS_ADDR;
   float   *pRouterScratch = ROUTER_SCRATCH_ADDR;
   float   *pRouterOut     = ROUTER_OUT_ADDR;
   // Parametric EQ pointers (using CascadeBiquad implementation)
   float *pParamEqFilterVar = PARAM_EQ_FILTER_VAR_ADDR;
   float *pParamEqOut       = PARAM_EQ_OUT_ADDR;

   // DelayNCh pointers
   int32_t *pDelayNChSizes   = (int32_t *) DELAY_NCH_SIZES_ADDR;
   float   *pDelayNChBuffer  = DELAY_NCH_BUFFER_ADDR;
   float   *pDelayNChScratch = DELAY_NCH_SCRATCH_ADDR;
   float   *pDelayNChOut     = DELAY_NCH_OUT_ADDR;

   // BalanceFader pointers
   float   *pBalanceFaderGain          = BALANCE_FADER_GAIN_ADDR;
   int32_t *pBalanceFaderChannelConfig = (int32_t *) BALANCE_FADER_CHANNEL_CONFIG_ADDR;
   float   *pBalanceFaderOut           = BALANCE_FADER_OUT_ADDR;

   // GainNChTrim pointers
   float *pGainNChTrimChannelGains = GAIN_NCH_TRIM_CHANNEL_GAINS_ADDR;
   float *pGainNChTrimMasterGain   = GAIN_NCH_TRIM_MASTER_GAIN_ADDR;
   float *pGainNChTrimOut          = GAIN_NCH_TRIM_OUT_ADDR;

   // Limiter pointers
   float *pLimiterScratch      = LIMITER_SCRATCH_ADDR;
   float *pLimiterGainState    = LIMITER_GAIN_STATE_ADDR;
   float *pLimiterThreshold    = LIMITER_THRESHOLD_ADDR;
   float *pLimiterKneeWidth    = LIMITER_KNEE_WIDTH_ADDR;
   float *pLimiterAttackCoeff  = LIMITER_ATTACK_COEFF_ADDR;
   float *pLimiterReleaseCoeff = LIMITER_RELEASE_COEFF_ADDR;
   float *pLimiterOut          = LIMITER_OUT_ADDR;

   // MuteNCh pointers
   int32_t *pMuteNChIsMute = (int32_t *) MUTE_NCH_IS_MUTE_ADDR;
   float   *pMuteNChOut    = MUTE_NCH_OUT_ADDR;

   // Final output pointer
   int32_t *pFloatToInt = FLOAT_TO_INT_ADDR;

   // Copy input data
   for (auto i = 0; i < BLOCK_SIZE * NUM_CHANNELS_IN; i++) {
      pIn[i] = inputSignalCase1[i];
   }

   // Copy cascade biquad coefficients
   for (auto i = 0; i < CASCADE_FILTER_SIZE; i++) {
      pCascadeCoeff[i] = cascadeBiquad0CoeffCase1[i];
   }

   // Initialize cascade biquad filter variables
   for (auto i = 0; i < CASCADE_FILTER_VAR_SIZE; i++) {
      pCascadeFilterVar[i] = 0;
   }

   // Initialize parametric EQ filter variables
   for (auto i = 0; i < PARAM_EQ_FILTER_VAR_SIZE; i++) {
      pParamEqFilterVar[i] = 0;
   }

   // Copy gain values for gainNCh
   for (auto i = 0; i < NUM_CHANNELS_IN; i++) {
      pGainNChTarget[i] = targetGain1[i];
   }

   // Copy router configuration
   for (auto i = 0; i < NUM_CHANNELS_OUT; i++) {
      pRouterConfig[i] = routerConfig1[i];
   }

   // Copy delay sizes from staticCase1.hpp
   for (auto i = 0; i < NUM_CHANNELS_OUT; i++) {
      pDelayNChSizes[i] = delaySizes1[i];
   }

   // Set up channel gains for gainNChTrim
   for (auto i = 0; i < NUM_CHANNELS_OUT; i++) {
      pGainNChTrimChannelGains[i] = channelGains1[i];
   }
   // Set master gain for gainNChTrim
   pGainNChTrimMasterGain[0] = 0.95f;

   // Set up limiter parameters
   for (auto i = 0; i < NUM_CHANNELS_OUT; i++) {
      pLimiterThreshold[i]    = threshold_dB1[i];
      pLimiterKneeWidth[i]    = kneeWidth_dB1[i];
      pLimiterAttackCoeff[i]  = attackCoeff1[i];
      pLimiterReleaseCoeff[i] = releaseCoeff1[i];
   }

   for (auto i = 0; i < NUM_CHANNELS_OUT; i++) {
      pBalanceFaderChannelConfig[i] = balanceFaderChannelConfig1[i];
      pBalanceFaderGain[i]          = 1.0f; // Initialize gains to 1.0 (unity gain) instead of 0
   }

   // Copy gain values for muteNCh
   for (auto i = 0; i < NUM_CHANNELS_OUT; i++) {
      pMuteNChIsMute[i] = 0; // 0 = unmuted, 1 = muted
   }

   printf("\n-------------------------------------------------");
   printf("\n  11.2 Signal Chain Demo using TISP");
   printf("\n-------------------------------------------------\n");

   printf("\nCreating Graph:\n");
   printf("\nL2-->typeconversion-->gainNCh-->cascadebiquad-->router-->parametric "
          "eq-->delayNCh-->balanceFader-->gainNChTrim-->limiter-->muteNCh-->typeconversion-->L2\n");

   // Create the signal chain
   void *myGraph = nullptr;
   auto  myOpVec = new TISP::opVec();

   // 1. TypeConversion: int32_t to float
   if (errorCtx.isSuccess()) {
      auto k0 = TISP::make_unique_node<TISP::AUDIOLIB::TypeConversion<int32_t, float>>(
          pIn, pTypeConvInOut, NUM_CHANNELS_IN, BLOCK_SIZE, NUM_CHANNELS_IN * sizeof(int32_t),
          NUM_CHANNELS_IN * sizeof(float), IS_INTERLEAVED, "TypeConversion (i32->f32)", 0, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k0));
      }
      else {
         printf("Error: Failed to create Node 0 (TypeConversion): %s\n", errorCtx.getMessage());
      }
   }

   // 2. GainNCh
   if (errorCtx.isSuccess()) {
      auto k1 = TISP::make_unique_node<TISP::AUDIOLIB::GainNCh<float>>(
          pTypeConvInOut, pGainNChTarget, pGainNChOut, NUM_CHANNELS_IN, BLOCK_SIZE, NUM_CHANNELS_IN,
          NUM_CHANNELS_IN * sizeof(float), NUM_CHANNELS_IN * sizeof(float), IS_INTERLEAVED, "GainNCh", 1, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k1));
      }
      else {
         printf("Error: Failed to create Node 1 (GainNCh): %s\n", errorCtx.getMessage());
      }
   }

   // 3. CascadeBiquad
   if (errorCtx.isSuccess()) {
      auto k2 = TISP::make_unique_node<TISP::DSPLIB::CascadeBiquad<float>>(
          pGainNChOut, pCascadeCoeff, pCascadeFilterVar, pCascadeOut, BLOCK_SIZE, NUM_CHANNELS_IN, CASCADE_NUM_STAGES,
          DSPLIB_BIQUAD_DF1, NUM_CHANNELS_IN * sizeof(float), NUM_CHANNELS_IN * sizeof(float),
          NUM_CHANNELS_IN * sizeof(float), false, "CascadeBiquad (HPF)", 2, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k2));
      }
      else {
         printf("Error: Failed to create Node 2 (CascadeBiquad): %s\n", errorCtx.getMessage());
      }
   }

   // 4. Router
   if (errorCtx.isSuccess()) {
      auto k3 = TISP::make_unique_node<TISP::AUDIOLIB::Router<float>>(
          pCascadeOut, (float *) pRouterConfig, (float *) pRouterScratch, (float *) pRouterOut, NUM_CHANNELS_IN,
          BLOCK_SIZE, NUM_CHANNELS_OUT, NUM_CHANNELS_IN * sizeof(float), NUM_CHANNELS_OUT * sizeof(float),
          IS_INTERLEAVED, "Router (8ch->12ch)", 3, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k3));
      }
      else {
         printf("Error: Failed to create Node 3 (Router): %s\n", errorCtx.getMessage());
      }
   }

   // 5. Parametric EQ (using CascadeBiquad)
   if (errorCtx.isSuccess()) {
      auto k4 = TISP::make_unique_node<TISP::DSPLIB::CascadeBiquad<float>>(
          pRouterOut, pCascadeCoeff, pParamEqFilterVar, pParamEqOut, BLOCK_SIZE, NUM_CHANNELS_OUT, CASCADE_NUM_STAGES,
          DSPLIB_BIQUAD_DF1, NUM_CHANNELS_OUT * sizeof(float), NUM_CHANNELS_OUT * sizeof(float),
          NUM_CHANNELS_OUT * sizeof(float), false, "CascadeBiquad (Speaker EQ)", 4, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k4));
      }
      else {
         printf("Error: Failed to create Node 4 (Parametric EQ): %s\n", errorCtx.getMessage());
      }
   }

   // 6. DelayNCh
   if (errorCtx.isSuccess()) {
      auto k5 = TISP::make_unique_node<TISP::AUDIOLIB::DelayNChannel<float>>(
          pParamEqOut, pDelayNChBuffer, pDelayNChScratch, pDelayNChOut, NUM_CHANNELS_OUT, BLOCK_SIZE,
          DELAY_MAX_DELAY + 1, NUM_CHANNELS_OUT, // width=BLOCK_SIZE, height=NUM_CHANNELS_OUT, widthD=DELAY_MAX_DELAY+1,
                                                 // heightD=NUM_CHANNELS_OUT
          NUM_CHANNELS_OUT * sizeof(float), (DELAY_MAX_DELAY + 1) * sizeof(float),
          NUM_CHANNELS_OUT * sizeof(float), // inPitchBytes, delayBytes, outPitchBytes
          IS_INTERLEAVED, DELAY_NCH_LINEAR_MODE, (uint32_t *) pDelayNChSizes, "DelayNChannel", 5,
          errorCtx); // interleave=IS_INTERLEAVED, mode=0, pDelaySize (max delay derived internally)
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k5));
      }
      else {
         printf("Error: Failed to create Node 5 (DelayNCh): %s\n", errorCtx.getMessage());
      }
   }

   // 7. BalanceFader
   if (errorCtx.isSuccess()) {
      auto k6 = TISP::make_unique_node<TISP::AUDIOLIB::BalanceFader<float>>(
          pDelayNChOut, pBalanceFaderGain, pBalanceFaderOut, NUM_CHANNELS_OUT, BLOCK_SIZE,
          NUM_CHANNELS_OUT, // gWidthIn set to NUM_CHANNELS_OUT
          NUM_CHANNELS_OUT * sizeof(float), NUM_CHANNELS_OUT * sizeof(float),
          IS_INTERLEAVED, // isInterleave=IS_INTERLEAVED
          balanceFaderFloatParams1[0], balanceFaderFloatParams1[1], pBalanceFaderChannelConfig,
          balanceFaderFloatParams1[2], balanceFaderFloatParams1[3], balanceFaderFloatParams1[4],
          balanceFaderIntParams1[0], balanceFaderIntParams1[1], "BalanceFader", 6, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k6));
      }
      else {
         printf("Error: Failed to create Node 6 (BalanceFader): %s\n", errorCtx.getMessage());
      }
   }

   // 8. GainNChTrim
   if (errorCtx.isSuccess()) {
      auto k7 = TISP::make_unique_node<TISP::AUDIOLIB::GainNChTrim<float>>(
          pBalanceFaderOut, pGainNChTrimChannelGains, pGainNChTrimMasterGain, pGainNChTrimOut, NUM_CHANNELS_OUT,
          BLOCK_SIZE, NUM_CHANNELS_OUT, // gHeight = NUM_CHANNELS_OUT
          NUM_CHANNELS_OUT * sizeof(float), NUM_CHANNELS_OUT * sizeof(float), IS_INTERLEAVED, "GainNChTrim", 7,
          errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k7));
      }
      else {
         printf("Error: Failed to create Node 7 (GainNChTrim): %s\n", errorCtx.getMessage());
      }
   }

   // 9. Limiter
   if (errorCtx.isSuccess()) {
      auto k8 = TISP::make_unique_node<TISP::AUDIOLIB::Limiter<float>>(
          pGainNChTrimOut, pLimiterScratch, pLimiterOut, pLimiterGainState, pLimiterThreshold, pLimiterKneeWidth,
          pLimiterAttackCoeff, pLimiterReleaseCoeff, NUM_CHANNELS_OUT, BLOCK_SIZE, NUM_CHANNELS_OUT * sizeof(float),
          NUM_CHANNELS_OUT * sizeof(float), MUTE_NCH_STRIDE_ELEM * sizeof(float), "Limiter", 8,
          errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k8));
      }
      else {
         printf("Error: Failed to create Node 8 (Limiter): %s\n", errorCtx.getMessage());
      }
   }

   // 10. MuteNCh
   if (errorCtx.isSuccess()) {
      auto k9 = TISP::make_unique_node<TISP::AUDIOLIB::Mutench<float>>(
          pLimiterOut, pMuteNChOut, NUM_CHANNELS_OUT, BLOCK_SIZE, NUM_CHANNELS_OUT,
          MUTE_NCH_STRIDE_ELEM * sizeof(float), MUTE_NCH_STRIDE_ELEM * sizeof(float), IS_INTERLEAVED, SAMPLING_RATE_HZ,
          (uint32_t *) pMuteNChIsMute, MUTE_NCH_FADE_TIME, MUTE_NCH_FADE_HARD_TYPE, "MuteNCh", 9, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k9));
      }
      else {
         printf("Error: Failed to create Node 9 (MuteNCh): %s\n", errorCtx.getMessage());
      }
   }

   // 11. TypeConversion: float to int32_t
   if (errorCtx.isSuccess()) {
      auto k10 = TISP::make_unique_node<TISP::AUDIOLIB::TypeConversion<float, int32_t>>(
          pMuteNChOut, pFloatToInt, NUM_CHANNELS_OUT, BLOCK_SIZE, MUTE_NCH_STRIDE_ELEM * sizeof(float),
          NUM_CHANNELS_OUT * sizeof(int32_t), IS_INTERLEAVED, "TypeConversion (f32->i32)", 10, errorCtx);
      if (errorCtx.isSuccess()) {
         myOpVec->push_back(std::move(k10));
      }
      else {
         printf("Error: Failed to create Node 10 (TypeConversion): %s\n", errorCtx.getMessage());
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
   int32_t *pOut = FLOAT_TO_INT_ADDR;

   TISP::opVec *myOpVec = static_cast<TISP::opVec *>(myGraph);
   size_t       length  = myOpVec->size();

   TISP::SetAddr_t addr;

   // Process data in blocks of BLOCK_SIZE
   for (auto k = 0; k < (NUM_SAMPLES) / (BLOCK_SIZE); k++) {
      // Copy input data for the current block
      for (auto j = 0; j < BLOCK_SIZE; j++) {
         for (auto i = 0; i < NUM_CHANNELS_IN; i++) {
            pIn[j * NUM_CHANNELS_IN + i] = inputSignalCase1[i + j * NUM_CHANNELS_IN + k * BLOCK_SIZE * NUM_CHANNELS_IN];
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

   // Write profiler results to json
   const char *signalChainName = "TISP_11_02_signal_chain";
   const char *extensionName   = ".json";
   char        fileName[100];

   // Copy the first string to the buffer
   strcpy(fileName, signalChainName);

   // Add the extension
   strcat(fileName, extensionName);

   // Node profiler
   NodeProfiler::printResults(*myOpVec);
   NodeProfiler::printMemoryUsage();
   NodeProfiler::writeJsonResults(fileName, *myOpVec);
}

void check_output()
{

#if DEBUG
   // Get the pointers from the memory map
   float   *pTypeConvInOut   = TYPE_CONV_IN_OUT_ADDR;
   float   *pGainNChOut      = GAIN_NCH_OUT_ADDR;
   float   *pCascadeOut      = CASCADE_OUT_ADDR;
   float   *pRouterOut       = ROUTER_OUT_ADDR;
   float   *pParamEqOut      = PARAM_EQ_OUT_ADDR;
   float   *pDelayNChOut     = DELAY_NCH_OUT_ADDR;
   float   *pBalanceFaderOut = BALANCE_FADER_OUT_ADDR;
   float   *pGainNChTrimOut  = GAIN_NCH_TRIM_OUT_ADDR;
   float   *pLimiterOut      = LIMITER_OUT_ADDR;
   float   *pMuteNChOut      = MUTE_NCH_OUT_ADDR;
   int32_t *pFloatToInt      = FLOAT_TO_INT_ADDR;

   // Print the values in the pointers in blocksize x numchannels format
   printf("\n1. Values in pTypeConvInOut (NUM_CHANNELS_IN=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_IN, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_IN; j++) {
         printf("%f ", pTypeConvInOut[i * NUM_CHANNELS_IN + j]);
      }
      printf("\n");
   }

   printf("\n2. Values in pGainNChOut (NUM_CHANNELS_IN=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_IN, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_IN; j++) {
         printf("%f ", pGainNChOut[i * NUM_CHANNELS_IN + j]);
      }
      printf("\n");
   }

   printf("\n3. Values in pCascadeOut (NUM_CHANNELS_IN=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_IN, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_IN; j++) {
         printf("%f ", pCascadeOut[i * NUM_CHANNELS_IN + j]);
      }
      printf("\n");
   }

   printf("\n4. Values in pRouterOut (NUM_CHANNELS_OUT=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_OUT, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%f ", pRouterOut[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

   printf("\n5. Values in pParamEqOut (NUM_CHANNELS_OUT=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_OUT, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%f ", pParamEqOut[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

   printf("\n6. Values in pDelayNChOut (NUM_CHANNELS_OUT=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_OUT, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%f ", pDelayNChOut[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

   printf("\n7. Values in pBalanceFaderOut (NUM_CHANNELS_OUT=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_OUT, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%f ", pBalanceFaderOut[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

   printf("\n8. Values in pGainNChTrimOut (NUM_CHANNELS_OUT=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_OUT, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%f ", pGainNChTrimOut[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

   printf("\n9. Values in pLimiterOut (NUM_CHANNELS_OUT=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_OUT, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%f ", pLimiterOut[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

   printf("\n10. Values in pMuteNChOut (NUM_CHANNELS_OUT=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_OUT, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%f ", pMuteNChOut[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

   // Print the isMute buffer values
   int32_t *pMuteNChIsMute = (int32_t *) MUTE_NCH_IS_MUTE_ADDR;
   printf("\nValues in pMuteNChIsMute (NUM_CHANNELS_OUT=%d):\n", NUM_CHANNELS_OUT);
   for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
      printf("%d ", pMuteNChIsMute[j]);
   }
   printf("\n");

   printf("\n11. Values in pFloatToInt (NUM_CHANNELS_OUT=%d, BLOCK_SIZE=%d):\n", NUM_CHANNELS_OUT, BLOCK_SIZE);
   for (int i = 0; i < BLOCK_SIZE; i++) {
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%d ", pFloatToInt[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

#endif // DEBUG

   int32_t       *pOut    = FLOAT_TO_INT_ADDR;
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

#if DEBUG
   // Print the output values
   printf("\nOutput values:\n");
   for (int i = 0; i < NUM_SAMPLES; i++) {
      printf("Sample %d: ", i);
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%d ", pOut[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

   printf("\nReference values:\n");
   for (int i = 0; i < NUM_SAMPLES; i++) {
      printf("Sample %d: ", i);
      for (int j = 0; j < NUM_CHANNELS_OUT; j++) {
         printf("%d ", pOutRef[i * NUM_CHANNELS_OUT + j]);
      }
      printf("\n");
   }

#endif // DEBUG

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
