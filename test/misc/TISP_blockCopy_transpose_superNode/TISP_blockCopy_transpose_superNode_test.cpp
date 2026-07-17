// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_superNode.hpp"
#include "TISP_test.h"
#include <TISP_ErrorCtxt.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <tisp.hpp>

#include "TISP_blockCopy_transpose_superNode_memory_map.hpp"

#include <vector>

#define TOTAL_DMA_CHANNELS (2)

void TISP_blockCopyTranspose_test_cn(float *restrict pX,
                                     float *restrict pY,
                                     size_t width,
                                     size_t height,
                                     size_t inPitch,
                                     size_t outPitch)
{
   for (uint32_t i = 0; i < width; i++) {
      for (uint32_t j = 0; j < height; j++) {
         pY[i * outPitch + j] = pX[j * inPitch + i];
      }
   }
}

#ifdef __cplusplus
extern "C" {
#endif
void *create_graph()
{

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

   uint32_t width  = WIDTH;
   uint32_t height = HEIGHT;

   uint32_t blockWidth  = BLOCK_WIDTH;
   uint32_t blockHeight = BLOCK_HEIGHT;

   float *input   = INPUT_ADDR;
   float *inputCn = INPUT_CN_ADDR;
   float *tempOut = TEMP_OUT_ADDR;
   float *output  = OUTPUT_ADDR;

   float *pInputBlock  = INPUT_BLOCK_ADDR;
   float *pOutputBlock = OUTPUT_BLOCK_ADDR;

   for (uint32_t i = 0; i < width * height; i++) {
      /* input[i] = (float)rand() / (float)(RAND_MAX / (2 * 8)) - 8; */
      input[i]   = i;
      inputCn[i] = input[i];
   }

   printf("\n-------------------------------------------------");
   printf("\n Transpose + block Copy Demo using TISP   ");
   printf("\n-------------------------------------------------\n");

   printf("\nCreating Graph:\n");
   printf("\nDDR-->L2-->matTrans-->DDR-->L2-->blockCopy-->L2-->DDR\n");

   TISP::ErrorCtxt errorCtx;

   auto k0 = std::make_unique<TISP::DSPLIB::MatTrans<float>>(pInputBlock, pOutputBlock, blockWidth, blockHeight,
                                                             blockWidth * sizeof(float), blockHeight * sizeof(float),
                                                             errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto myOpVec0 = std::make_unique<TISP::opVec>();

   myOpVec0->push_back(std::move(k0));

   auto s0 = std::make_unique<TISP::SuperNode::I2dToO2dTranspose<float, float>>(
       std::move(myOpVec0), input, tempOut, pInputBlock, pOutputBlock, width, height, blockWidth, blockHeight,
       width * sizeof(float), height * sizeof(float), (TOTAL_DMA_CHANNELS));

   auto k1 =
       std::make_unique<TISP::misc::BlockCopy<float>>(pInputBlock, pOutputBlock, blockHeight, blockWidth, errorCtx);

   if (!errorCtx.isSuccess()) {
      printf("\nError creating kernel: %s\n", errorCtx.getMessage());
      return nullptr;
   }

   auto myOpVec1 = std::make_unique<TISP::opVec>();

   myOpVec1->push_back(std::move(k1));

   auto s1 = std::make_unique<TISP::SuperNode::I2dToO2d<float, float>>(
       std::move(myOpVec1), tempOut, output, pInputBlock, pOutputBlock, height, width, blockHeight, blockWidth,
       height * sizeof(float), width * sizeof(float), (TOTAL_DMA_CHANNELS));

   auto mySuperNodeVec = new TISP::SuperNode::superNodeVec();

   mySuperNodeVec->push_back(std::move(s0));
   mySuperNodeVec->push_back(std::move(s1));

   void *myGraph = mySuperNodeVec;

   return myGraph;
}

void execute_graph(void *myGraph)
{

   printf("\nExecuting block copy transpose demo %d times\n", NUM_REPS);

#if (!HOST_EMULATION)
   uint64_t startTime, endTime;
   startTime = (__get_GTSC(1), __STSC);
#endif

   TISP::SuperNode::superNodeVec *mySuperNodeVec = static_cast<TISP::SuperNode::superNodeVec *>(myGraph);

   for (int32_t i = 0; i < NUM_REPS; i++) {
      TISP::execute::graph(mySuperNodeVec);
   }

#if (!HOST_EMULATION)
   endTime            = (__get_GTSC(1), __STSC);
   uint64_t totalTime = (endTime - startTime) / (NUM_REPS);
   printf("Average cycles to execute graph = %llu\n", totalTime);
   printf("Bandwidth: %.2f GB/sec\n", ((float) 2 * WIDTH * HEIGHT * sizeof(float)) / (totalTime));
#endif

   // Print the profiling results
   NodeProfiler::printResults();

   return;
}
void check_output()
{

   uint32_t width  = WIDTH;
   uint32_t height = HEIGHT;

   float *inputCn = INPUT_CN_ADDR;
   float *output  = OUTPUT_ADDR;

   float *refOut = REF_OUT_ADDR;

   TISP_blockCopyTranspose_test_cn(inputCn, refOut, width, height, width, width);

   int32_t status = TI_compare_mem_2D_float((void *) output, (void *) refOut, 0, (double) powf(2, -2), width, height,
                                            width * sizeof(float), sizeof(float));

   if (status == TI_TEST_KERNEL_PASS) {
      printf("Opt and NatC match!  PASSED! \n");
   }
   else {
      printf("Opt and NatC DON'T match!  FAILED!!! \n");
   }
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
