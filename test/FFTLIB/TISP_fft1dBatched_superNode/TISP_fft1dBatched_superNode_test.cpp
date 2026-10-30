// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_fft1dBatched.hpp"
#include "TISP_superNode.hpp"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tisp.hpp>

#include "TISP_fft1dBatched_superNode_memory_map.hpp"
#include <vector>

#define TOTAL_DMA_CHANNELS (2)

// Below cycle count assume flat memory model, i.e, no memory-related stalls
#define FFTLIB_FFT1DBATCHED_16X512_CYCLES (12904)
#define FFTLIB_FFT1DATCHED_512X512_CYCLES (FFTLIB_FFT1DBATCHED_16X512_CYCLES) * (512 / 16)
#define ESITMATED_TOTAL_CYCLE_COUNT ((FFTLIB_FFT1DATCHED_512X512_CYCLES))

void TISP_fft1dBatched_test_cn(float *pX, float *pW, float *pY, size_t numPoints, size_t numChannels);

#ifdef __cplusplus
extern "C" {
#endif
void *create_graph()
{

   NodeProfiler::initialize();
#ifdef HOST_EMULATION
#if defined(_MSC_VER)
   /* pIntMmeBase = (uint8_t *) _aligned_malloc(L2SRAM_SIZE, L2SRAM_SIZE); */
#else
   /* pIntMmeBase     = (uint8_t *) memalign(128, L2SRAM_SIZE); */
#endif
#elif __C7X_VEC_SIZE_BITS__ == 512
   int32_t                retVal = 0;
   Sciclient_ConfigPrms_t sciClientCfg;
   Sciclient_configPrmsInit(&sciClientCfg);
   retVal = Sciclient_init(&sciClientCfg);
   if (retVal != 0) {
      printf("Sciclient Init Failed \n");
   }

   TISP::test_sciclientDmscGetVersion(NULL, 0);
   TISP::appC7xClecInitDru();
#else
   App_dmautilsClecInitDru();
#endif

   uint32_t numPoints   = NUM_POINTS;
   uint32_t numChannels = NUM_CHANNELS;

   uint32_t blockWidth  = BLOCK_WIDTH;
   uint32_t blockHeight = BLOCK_HEIGHT;

   float *input   = INPUT_ADDR;
   float *inputCn = INPUT_CN_ADDR;
   float *output  = OUTPUT_ADDR;

   float *pInputBlock  = INPUT_BLOCK_ADDR;
   float *pOutputBlock = OUT_BLOCK_ADDR;

   /* float *pYCn = pXCn + (numPoints << 1) * numChannels; */

   for (uint32_t i = 0; i < numChannels * numPoints * 2; i++) {
      input[i]   = (float) rand() / (float) (RAND_MAX / (2 * 8)) - 8;
      inputCn[i] = input[i];
   }

   printf("\n------------------------------------------------------");
   printf("\n        FFT 1D Batched Demo using TISP                 ");
   printf("\n------------------------------------------------------\n");

   printf("\nCreating Graph:\n");
   printf("\nDDR-->L2-->fft1dBatched-->L2-->DDR\n");

   TISP::ErrorCtxt errorCtx;

   const TISP::TISP_BufDesc2D fftIn  = {pInputBlock, TISP::Types::kFloat32, numPoints * 2U, blockHeight,
                                      static_cast<size_t>(numPoints * 2U) * sizeof(float), 0U};
   const TISP::TISP_BufDesc2D fftOut = {pOutputBlock, TISP::Types::kFloat32, numPoints * 2U, blockHeight,
                                      static_cast<size_t>(numPoints * 2U) * sizeof(float), 0U};

   const TISP::TISP_BufDescList fftInputs  = {&fftIn, 1U};
   const TISP::TISP_BufDescList fftOutputs = {&fftOut, 1U};

   auto k0 = std::make_unique<TISP::FFTLIB::FFT1dBatched<float>>(&fftInputs, &fftOutputs, "FFT1dBatched", 0, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto myOpVec = std::make_unique<TISP::opVec>();

   myOpVec->push_back(std::move(k0));

   auto s0 = std::make_unique<TISP::SuperNode::I2dToO2d<float, float>>(
       std::move(myOpVec), input, output, pInputBlock, pOutputBlock, numPoints << 1, numChannels, blockWidth,
       blockHeight, blockWidth * sizeof(float), blockWidth * sizeof(float), (TOTAL_DMA_CHANNELS));

   auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

   mySuperNodeVec->push_back(std::move(s0));

   void *myGraph = mySuperNodeVec;

   return myGraph;
}

void execute_graph(void *myGraph)
{

   printf("\nExecuting demo %d times\n", NUM_REPS);

#if (!HOST_EMULATION)
   uint64_t startTime, endTime;
   startTime = (__get_GTSC(1), __STSC);
#endif

   for (int32_t i = 0; i < NUM_REPS; i++) {
      TISP::SuperNode::superNodeVec *mySuperNodeVec = static_cast<TISP::SuperNode::superNodeVec *>(myGraph);

      TISP::execute::graph(mySuperNodeVec);
   }

#if (!HOST_EMULATION)
   endTime            = (__get_GTSC(1), __STSC);
   uint64_t totalTime = (endTime - startTime) / (NUM_REPS);
   printf("Average cycles to execute graph = %llu\n", totalTime);

   printf("\nTheoritical cycle count (assuming flat memory w/ no stalls): %d\n", (ESITMATED_TOTAL_CYCLE_COUNT));
   printf("\nObeserved/Theoritical: %.2f\n", (((float) totalTime) / (ESITMATED_TOTAL_CYCLE_COUNT)));

   printf("\nPeformance of 1D Bathced FFT: %.2f GB/sec\n", (8 * NUM_POINTS * NUM_CHANNELS) / ((float) totalTime));
#endif
   NodeProfiler::printResults();
}
void check_output()
{

   uint32_t numPoints   = NUM_POINTS;
   uint32_t numChannels = NUM_CHANNELS;

   float *inputCn = INPUT_CN_ADDR;
   float *output  = OUTPUT_ADDR;

   float *refOut = REF_OUT_ADDR;
   float *pWCn   = TW_CN_ADDR;

   TISP_fft1dBatched_test_cn(inputCn, pWCn, refOut, numPoints, numChannels);

   int32_t status = TI_compare_mem_2D_float((void *) output, (void *) refOut, 0, (double) powf(2, -2), numPoints << 1,
                                            numChannels, (numPoints << 1) * sizeof(float), sizeof(float));

   if (status == TI_TEST_KERNEL_PASS) {
      printf("Opt and NatC match!  PASSED! \n");
   }
   else {
      printf("Opt and NatC DON'T match!  FAILED!!! \n");
   }

   printf("\nFreeing resources...\n");
}

void delete_graph(void *myGraph)
{
   TISP::SuperNode::superNodeVec *mySuperNodeVec = static_cast<TISP::SuperNode::superNodeVec *>(myGraph);

   delete (mySuperNodeVec);

   return;
}

#ifdef __cplusplus
}
#endif
