// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_blockCopy.hpp"
#include "TISP_fft2d_superNode_memory_map.hpp"
#include "TISP_superNode.hpp"
#include "TISP_test.h"
#include "TISP_util.hpp"
#include "TI_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <cstdint>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tisp.hpp>

using namespace std;

#include <vector>

#define TOTAL_DMA_CHANNELS (2)

// Below cycle count assume flat memory model, i.e, no memory-related stalls
#define FFTLIB_FFT1DBATCHED_16X512_CYCLES (12904)
#define FFTLIB_FFT1DATCHED_512X512_CYCLES (FFTLIB_FFT1DBATCHED_16X512_CYCLES) * (512 / 16)
#define DSPLIB_MAT_TRANS_CYCLES ((NUM_POINTS * NUM_CHANNELS) / 4)

#define ESITMATED_TOTAL_CYCLE_COUNT ((2 * FFTLIB_FFT1DATCHED_512X512_CYCLES) + DSPLIB_MAT_TRANS_CYCLES)

void TISP_fft2d_test_cn(float *pX, float *pW, float *pTemp, float *pY, size_t numPoints, size_t numChannels);

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
   float *tempOut = TEMP_OUT_ADDR;
   float *output  = OUTPUT_ADDR;

   float *pInputBlock  = INPUT_BLOCK_ADDR;
   float *pTempBlock   = TEMP_BLOCK_ADDR;
   float *pOutputBlock = OUT_BLOCK_ADDR;

   /* FILE *inputFile; */
   /* FILE *refOutFile; */

   /* inputFile  = fopen("./data/FFTLIB/input_32fc.bin", "r"); */
   /* refOutFile = fopen("./data/FFTLIB/refOut_32fc.bin", "r"); */

   /* printf("\nReading input data\n"); */
   /* fread(input, sizeof(double), numChannels * numPoints, inputFile); */

   for (uint32_t i = 0; i < numChannels * numPoints * 2; i++) {
      input[i]   = (float) rand() / (float) (RAND_MAX / (2 * 8)) - 8;
      inputCn[i] = input[i];
   }

   /* printf("\nReading ref data\n"); */
   /* fread(refOut, sizeof(double), numChannels * numPoints, refOutFile); */

   printf("\n------------------------------------------------");
   printf("\n         2D FFT Demo using TISP (%d x %d )      ", (NUM_CHANNELS), (NUM_POINTS));
   printf("\n------------------------------------------------\n");

   printf("\nCreating Graph using smart pointers and no DMA params in node creation\n");
   printf("\nDDR-->L2-->1DBatchedFFT-->L2-->MatrixTranspose-->L2-->DDR-->L2-->1DBatchedFFT-->L2-->DDR\n");

   //! [2D FFT Example]

   TISP::ErrorCtxt errorCtx;

   auto k0 = TISP::make_unique_node<TISP::FFTLIB::FFT1dBatched<float>>(pInputBlock, pTempBlock, numPoints, blockHeight,
                                                                       "FFT1dBatched_FirstPass", 1001, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto k1 = TISP::make_unique_node<TISP::DSPLIB::MatTrans<double>>(
       (double *) pTempBlock, (double *) pOutputBlock, blockWidth >> 1, blockHeight, blockWidth * sizeof(double) >> 1,
       blockHeight * sizeof(double), "MatrixTranspose", 1002, errorCtx);

   auto myOpVec0 = make_unique<TISP::opVec>();

   myOpVec0->push_back(std::move(k0));
   myOpVec0->push_back(std::move(k1));

   auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<double, double>>(
       std::move(myOpVec0), (double *) input, (double *) tempOut, (double *) pInputBlock, (double *) pOutputBlock,
       numPoints, numChannels, blockWidth >> 1, blockHeight, (numPoints) * sizeof(double), numChannels * sizeof(double),
       (TOTAL_DMA_CHANNELS));

   auto k2 = TISP::make_unique_node<TISP::FFTLIB::FFT1dBatched<float>>(
       pInputBlock, pOutputBlock, numChannels, blockHeight, "FFT1dBatched_SecondPass", 1003, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto myOpVec1 = std::make_unique<TISP::opVec>();

   myOpVec1->push_back(std::move(k2));

   auto s1 = std::make_unique<TISP::SuperNode::I2dToO2d<float, float>>(
       std::move(myOpVec1), tempOut, output, pInputBlock, pOutputBlock, numChannels << 1, numPoints, numChannels << 1,
       blockHeight, (numChannels << 1) * sizeof(float), (numChannels << 1) * sizeof(float), (TOTAL_DMA_CHANNELS));

   auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

   mySuperNodeVec->push_back(std::move(s0));
   mySuperNodeVec->push_back(std::move(s1));

   //! [2D FFT Example]

   // C-callable pointer to mySuperNodeVec that has all the info about the graph
   // prevents usage of smart pointer
   void *myGraph = mySuperNodeVec;

   return myGraph;
}

void execute_graph(void *myGraph)
{

   printf("\nExecuting 2D FFt %d times\n", NUM_REPS);

   TISP::SuperNode::superNodeVec *mySuperNodeVec = static_cast<TISP::SuperNode::superNodeVec *>(myGraph);

#if (!HOST_EMULATION)
   uint64_t startTime, endTime;
   startTime = (__get_GTSC(1), __STSC);
#endif

   for (int32_t i = 0; i < NUM_REPS; i++) {
      NodeProfiler::resetCounters();
      TISP::execute::graph(mySuperNodeVec);
   }

#if (!HOST_EMULATION)
   endTime            = (__get_GTSC(1), __STSC);
   uint64_t totalTime = (endTime - startTime) / (NUM_REPS);
   printf("Average cycles to execute graph = %llu\n", totalTime);

   printf("\nTheoritical cycle count (assuming flat memory w/ no stalls): %d\n", (ESITMATED_TOTAL_CYCLE_COUNT));
   printf("\nObeserved/Theoritical: %.2f\n", (((float) totalTime) / (ESITMATED_TOTAL_CYCLE_COUNT)));
   printf("\nMemory performance of 2D FFT: %.2f GB/sec\n", (8 * NUM_POINTS * NUM_CHANNELS * 2) / ((float) totalTime));
#endif

   // Print the profiling results
   NodeProfiler::printResults();

   const char *jsonFilename = "profiling_results.json";
   NodeProfiler::writeJsonResults(jsonFilename, *mySuperNodeVec);
}

void delete_graph(void *myGraph)
{

   TISP::SuperNode::superNodeVec *mySuperNodeVec = static_cast<TISP::SuperNode::superNodeVec *>(myGraph);

   delete (mySuperNodeVec);

   return;
}

void check_output()
{

   uint32_t numPoints   = NUM_POINTS;
   uint32_t numChannels = NUM_CHANNELS;

   float *inputCn   = INPUT_CN_ADDR;
   float *tempOutCn = TEMP_OUT_CN_ADDR;
   float *output    = OUTPUT_ADDR;

   float *refOut = REF_OUT_ADDR;

   float *pWCn = TW_CN_ADDR;

   printf("\nRunning Natc for 2D FFT\n");
   TISP_fft2d_test_cn(inputCn, pWCn, tempOutCn, refOut, numPoints, numChannels);

   int32_t status = TI_compare_mem_2D_float((void *) output, (void *) refOut, 0, (double) powf(2, -2), numChannels << 1,
                                            numPoints, (numChannels << 1) * sizeof(float), sizeof(float));

   if (status == TI_TEST_KERNEL_PASS) {
      printf("Opt and Ref match!  PASSED! \n");
   }
   else {
      printf("Opt and Ref DON'T match!  FAILED!!! \n");
   }

   return;
}
#ifdef __cplusplus
}
#endif
