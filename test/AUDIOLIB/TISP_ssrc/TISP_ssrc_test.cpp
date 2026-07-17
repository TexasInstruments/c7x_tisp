// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/* include AUDIOLIB */
#include <audiolib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <tisp.hpp>

/* include test data for this kernel */
#include "TISP_ssrc_cn.h"
#include "TISP_ssrc_idat.h"
#include "common/AUDIOLIB_bufParams.h"

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) /* 1 MB */
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) /* 800 KB */
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) /* 2MB KB */
#endif

#ifdef WIN32
#if defined(__C7504__)
int8_t      l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t      ddrBuffer[2048 * 1024];
const float audiolib_ssrc_downsampleStage1[] = {
#include "filt_coeffs/halfband_filter_stage1_downsample2x_rfmt.h"
};
const float audiolib_ssrc_downsampleStage2[] = {
#include "filt_coeffs/halfband_filter_stage2_downsample2x_rfmt.h"
};
const float audiolib_ssrc_upsampleStage1[] = {
#include "filt_coeffs/halfband_filter_stage1_upsample2x_rfmt.h"
};
const float audiolib_ssrc_upsampleStage2[] = {
#include "filt_coeffs/halfband_filter_stage2_upsample2x_rfmt.h"
};
#elif defined(__C7524__)
int8_t      l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t      ddrBuffer[1 * 1024 * 1024];
const float audiolib_ssrc_downsampleStage1[] = {
#include "filt_coeffs/halfband_filter_stage1_downsample2x_rfmt.h"
};
const float audiolib_ssrc_downsampleStage2[] = {
#include "filt_coeffs/halfband_filter_stage2_downsample2x_rfmt.h"
};
const float audiolib_ssrc_upsampleStage1[] = {
#include "filt_coeffs/halfband_filter_stage1_upsample2x_rfmt.h"
};
const float audiolib_ssrc_upsampleStage2[] = {
#include "filt_coeffs/halfband_filter_stage2_upsample2x_rfmt.h"
};
#else
int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
int8_t ddrBuffer[2048 * 1024];
#endif
#else
#if defined(__C7504__)
__attribute__((section(".l2sramaux"), aligned(64))) int8_t  l2auxBuffer[TISP_L2_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t    ddrBuffer[2048 * 1024];
__attribute__((section(".l2sramData"), aligned(64))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];

__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_ssrc_downsampleStage1[] = {
#include "filt_coeffs/halfband_filter_stage1_downsample2x_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_ssrc_downsampleStage2[] = {
#include "filt_coeffs/halfband_filter_stage2_downsample2x_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_ssrc_upsampleStage1[] = {
#include "filt_coeffs/halfband_filter_stage1_upsample2x_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_ssrc_upsampleStage2[] = {
#include "filt_coeffs/halfband_filter_stage2_upsample2x_rfmt.h"
};

#elif defined(__C7524__)
__attribute__((section(".ddrData"), aligned(128))) int8_t    ddrBuffer[1 * 1024 * 1024];
__attribute__((section(".l2sramData"), aligned(128))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];

__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_ssrc_downsampleStage1[] = {
#include "filt_coeffs/halfband_filter_stage1_downsample2x_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_ssrc_downsampleStage2[] = {
#include "filt_coeffs/halfband_filter_stage2_downsample2x_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_ssrc_upsampleStage1[] = {
#include "filt_coeffs/halfband_filter_stage1_upsample2x_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_ssrc_upsampleStage2[] = {
#include "filt_coeffs/halfband_filter_stage2_upsample2x_rfmt.h"
};
#else
__attribute__((section(".msmcData"), aligned(64))) int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t  ddrBuffer[2048 * 1024];
#endif
#endif /* WIN32  */

/* define used to allocate ping-pong buffers */
#define AUDIOLIB_SSRC_TEST_PING_PONG_FACTOR (2U)

/* Number of warm reps */
#define AUDIOLIB_SSRC_NUM_WARM_REPS (10U)

int16_t volatile volatileSum = 0; /* use volatile to keep compiler from removing this operation  */

#ifdef ENABLE_GENERATE_OUTPUT_FILE
static void writeInOutDataToFile(float  *pOut,
                                 int32_t numChannels,
                                 int32_t sampleCount,
                                 int32_t testId,
                                 int32_t blockNum,
                                 int32_t isInterleaved,
                                 int32_t isInput)
{
   char  filename[256];
   FILE *fp;
   int   s, c;

   if (isInput) {
      sprintf(filename, "ssrc_testcase%d_input.csv", testId);
   }
   else {
      sprintf(filename, "ssrc_testcase%d_output.csv", testId);
   }

   if (blockNum == 0) {
      /* First block - create new file */
      fp = fopen(filename, "w");
   }
   else {
      /* Append to existing file */
      fp = fopen(filename, "a");
   }

   if (fp) {
      /* Always write in interleaved format to the file */
      for (s = 0; s < sampleCount; s++) {
         for (c = 0; c < numChannels; c++) {
            if (isInterleaved) {
               /* For interleaved data: [ch0,ch1,ch0,ch1,...] */
               fprintf(fp, "%.21g", pOut[s * numChannels + c]);
            }
            else {
               /* For non-interleaved data: [ch0,ch0,...,ch1,ch1,...] */
               fprintf(fp, "%.21g", pOut[c * sampleCount + s]);
            }
            /* Print a comma only if it's not the last column */
            if (c < numChannels - 1) {
               fprintf(fp, ", ");
            }
         }
         fprintf(fp, "\n");
      }

      fclose(fp);
   }
}
#endif

std::unique_ptr<TISP::VirtualOp> TISP_ssrc_createKernelObject(void            *pIn,
                                                              void            *pState,
                                                              void            *pFiltCoeffs,
                                                              void            *pOut,
                                                              uint32_t         inputSampleCount,
                                                              uint32_t         sampleDataType,
                                                              int32_t          widthIn,
                                                              int32_t          heightIn,
                                                              int32_t          strideIn,
                                                              int32_t          widthOut,
                                                              int32_t          heightOut,
                                                              int32_t          strideOut,
                                                              uint32_t         inputSampleRate,
                                                              uint32_t         outputSampleRate,
                                                              int32_t          numChannels,
                                                              uint8_t          isInterleave,
                                                              uint8_t          bufferFormat,
                                                              uint32_t         enableMMA,
                                                              uint8_t          mmaSize,
                                                              TISP::ErrorCtxt &errorCtx)
{
   if (sampleDataType == AUDIOLIB_FLOAT32) {
      return std::make_unique<TISP::AUDIOLIB::Ssrc<float>>(
          (float *) pIn, (float *) pState, (float *) pFiltCoeffs, (float *) pOut, inputSampleCount, sampleDataType,
          widthIn, heightIn, strideIn, widthOut, heightOut, strideOut, inputSampleRate, outputSampleRate, numChannels,
          isInterleave, bufferFormat, enableMMA, mmaSize, errorCtx);
   }
   else {
      errorCtx.setInvalidParameterError("TISP_ssrc_createKernelObject", "unsupported data type for Ssrc kernel");
      return nullptr;
   }
}

int TISP_ssrc_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t k = 0;
   uint32_t testNum, cirBuffAddressMask = 0;
   uint64_t archCycles = 0;

   int32_t filtCoeffSizeBytes, outFrameLength;
   float  *pInData, *pOutData;
   float  *pInDBuffer;

   int32_t  totalOutputSampleCnt;
   uint32_t pInStartOffset;

   TISP_ssrc_testParams_t *prm;
   TISP_ssrc_testParams_t  currPrm;
   TISP_ssrc_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_ssrc");

   /* file IO for benchmarking */
   FILE *fpOutputCSV = fopen("TISP_ssrc.csv", "w");
   fprintf(fpOutputCSV, "Kernel, Test ID, Test type, "
                        "Data size, Arch cycles, EVM "
                        "cycles, EVM/Arch cycles, Pass/Fail\n");

   for (tpi = 0; tpi < test_cases; tpi++) {
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      /* creates custom heap object */
      UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

      TISP_ssrcStateParams ssrcStateParams = {0};

      for (repCount = 0; repCount < numReps; repCount++) {
         int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_nat = TI_TEST_KERNEL_FAIL;

         uint32_t inBufferCbSizeInterleaved     = 0;
         uint32_t inBufferCbElementsInterleaved = 0;
         size_t   bufferFormat                  = 0;
         bool     setStatus                     = true;

         void    *pState __attribute__((unused)) = nullptr;
         int32_t  inputSize, pInAlign;
         void    *pOutCn;
         void    *pFiltCoeffs;
         uint32_t status = 0;
         uint32_t block;

         int32_t comparisonDone = 0;
         currentTestFail        = 0;

         int32_t            outputSampleCount = 0;
         uint32_t           inputSampleCount  = currPrm.inputSampleCount;
         ssrc_sample_rate_t inputSampleRate   = (ssrc_sample_rate_t) currPrm.inputSampleRate;
         ssrc_sample_rate_t outputSampleRate  = (ssrc_sample_rate_t) currPrm.outputSampleRate;
         uint8_t            numChannels       = currPrm.numChannels;
         uint8_t            isInterleave      = currPrm.dataFormat;
         uint8_t            enableMMA         = currPrm.enableMMA;
         uint32_t           mmaSize           = DSPLIB_MMA_SIZE_32_BIT;
         uint32_t           numBlocks         = currPrm.numBlocks;
         int32_t            eleSize           = AUDIOLIB_sizeof(currPrm.sampleDataType);
         uint32_t           alignedEleCount   = AUDIOLIB_L2DATA_ALIGNMENT / eleSize;
         bufferFormat = AUDIOLIB_ssrc_optimalBufferFormat(inputSampleRate, outputSampleRate, inputSampleCount,
                                                          numChannels, isInterleave, enableMMA);
         size_t widthIn, heightIn, widthOut, heightOut, strideIn, strideOut;

         if (bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) {
            enableMMA = 0;
         }

         outFrameLength = AUDIOLIB_ssrc_getOutBufferLength(inputSampleRate, outputSampleRate, inputSampleCount);

         if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
            if (bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) {
               AUDIOLIB_ssrc_bufferParams_t cbParams;
               status = AUDIOLIB_ssrc_getCircularBufferParams(inputSampleRate, outputSampleRate, inputSampleCount,
                                                              numChannels, currPrm.sampleDataType, &cbParams);

               /* Store these values for later use in the code */
               inBufferCbElementsInterleaved = cbParams.alignment / AUDIOLIB_sizeof(currPrm.sampleDataType);
               inBufferCbSizeInterleaved     = cbParams.alignment;

               bool     isUpsamplingCb   = AUDIOLIB_ssrc_isUpsampling(inputSampleRate, outputSampleRate);
               float    sampleRateRatiof = AUDIOLIB_ssrc_getSampleRateRatio(inputSampleRate, outputSampleRate);
               uint32_t sampleRateRatio  = (uint32_t) sampleRateRatiof;

               heightIn = inputSampleCount;
               widthIn  = numChannels;
               strideIn = numChannels * AUDIOLIB_sizeof(currPrm.sampleDataType);

               inputSize = cbParams.bufferSize;
               pInAlign  = cbParams.alignment;

               pState = nullptr;

               heightOut = outFrameLength;
               widthOut  = numChannels;
               strideOut = numChannels * AUDIOLIB_sizeof(currPrm.sampleDataType);

               uint32_t historyLengthStage1 =
                   AUDIOLIB_ssrc_getSampleHistoryLength(inputSampleRate, outputSampleRate, 1U);
               uint32_t historyLengthStage2 =
                   AUDIOLIB_ssrc_getSampleHistoryLength(inputSampleRate, outputSampleRate, 2U);

               uint32_t filterLengthStage1 = AUDIOLIB_ssrc_getFilterLength(inputSampleRate, outputSampleRate, 1U);

               if (isUpsamplingCb) {
                  outputSampleCount = (int32_t) ceil(inputSampleCount * sampleRateRatio);
               }
               else {
                  outputSampleCount = (int32_t) (inputSampleCount / sampleRateRatio);
               }

               ssrcStateParams.stage1FiltCoeffsOffset = 0;
               ssrcStateParams.stage2FiltCoeffsOffset =
                   AUDIOLIB_ceilingDiv(filterLengthStage1, alignedEleCount) * alignedEleCount;

               ssrcStateParams.inBufferTotalDimX = cbParams.alignment / AUDIOLIB_sizeof(currPrm.sampleDataType);

               ssrcStateParams.stage1BuffStartIndex =
                   (int32_t) ssrcStateParams.inBufferTotalDimX - (int32_t) historyLengthStage1 * widthIn;
               ssrcStateParams.stage2BuffStartIndex =
                   (int32_t) ssrcStateParams.inBufferTotalDimX - (int32_t) historyLengthStage2 * widthIn;

               cirBuffAddressMask                  = ssrcStateParams.inBufferTotalDimX - 1;
               ssrcStateParams.stage2BuffCurrIndex = 0;
            }
            else {
               heightIn = inputSampleCount;
               widthIn  = numChannels;
               strideIn = numChannels * AUDIOLIB_sizeof(currPrm.sampleDataType);

               AUDIOLIB_ssrc_bufferParams_t lbParams;
               status =
                   AUDIOLIB_ssrc_getStateBufferParams(inputSampleRate, outputSampleRate, inputSampleCount, numChannels,
                                                      currPrm.sampleDataType, isInterleave, &lbParams);

               uint32_t inputBufferSize =
                   AUDIOLIB_ssrc_getLinearInputBufferSize(inputSampleRate, outputSampleRate, inputSampleCount,
                                                          numChannels, currPrm.sampleDataType, isInterleave);

               inputSize = inputBufferSize;
               pInAlign  = 64;

               pState = (void *) TI_memalign(AUDIOLIB_L2DATA_ALIGNMENT, lbParams.bufferSize);

               heightOut = outFrameLength;
               widthOut  = numChannels;
               strideOut = numChannels * AUDIOLIB_sizeof(currPrm.sampleDataType);
            }
         }
         else {
            heightIn = numChannels;
            widthIn  = inputSampleCount;
            strideIn = inputSampleCount * AUDIOLIB_sizeof(currPrm.sampleDataType);

            AUDIOLIB_ssrc_bufferParams_t lbParams;
            status = AUDIOLIB_ssrc_getStateBufferParams(inputSampleRate, outputSampleRate, inputSampleCount,
                                                        numChannels, currPrm.sampleDataType, isInterleave, &lbParams);

            ssrcStateParams.inBufferTotalDimX = lbParams.alignment / eleSize;

            if (bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) {
               cirBuffAddressMask = ssrcStateParams.inBufferTotalDimX - 1;
            }

            uint32_t inputBufferSize = AUDIOLIB_ssrc_getLinearInputBufferSize(
                inputSampleRate, outputSampleRate, inputSampleCount, numChannels, currPrm.sampleDataType, isInterleave);

            inputSize = inputBufferSize;
            pInAlign  = 64;

            pState = (void *) TI_memalign(AUDIOLIB_L2DATA_ALIGNMENT, lbParams.bufferSize);

            heightOut = numChannels;
            widthOut  = outFrameLength;
            strideOut = outFrameLength * AUDIOLIB_sizeof(currPrm.sampleDataType);
         }

         filtCoeffSizeBytes = AUDIOLIB_ssrc_getFilterCoeffSize(inputSampleRate, outputSampleRate,
                                                               (AUDIOLIB_ssrc_buffer_format_t) bufferFormat, enableMMA,
                                                               mmaSize, currPrm.sampleDataType);

         pFiltCoeffs = (void *) TI_memalign(AUDIOLIB_L2DATA_ALIGNMENT, filtCoeffSizeBytes);

         auto pIn  = myL2Heap.allocateCustomAligned<uint8_t>(numBlocks * inputSize, pInAlign);
         auto pOut = myL2Heap.allocate64ByteAligned<uint8_t>(numBlocks * strideOut * heightOut);

         AUDIOLIB_ssrc_copyFilterCoeffs(inputSampleRate, outputSampleRate, (AUDIOLIB_ssrc_buffer_format_t) bufferFormat,
                                        enableMMA, mmaSize, currPrm.sampleDataType, audiolib_ssrc_downsampleStage1,
                                        audiolib_ssrc_downsampleStage2, audiolib_ssrc_upsampleStage1,
                                        audiolib_ssrc_upsampleStage2, filtCoeffSizeBytes, pFiltCoeffs);
         float *pFilter = (float *) pFiltCoeffs;

         pOutCn = (void *) ddrBuffer;
         memset(pOutCn, 0, numBlocks * strideOut * heightOut);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pOut.get() && pOutCn && pFiltCoeffs &&
             ((pState == nullptr && bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) ||
              (pState != nullptr && bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_LINEAR))) {

            pInStartOffset = 0;

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;

            auto k0 = TISP_ssrc_createKernelObject(
                pIn.get(), pState, pFiltCoeffs, pOut.get(), static_cast<uint32_t>(inputSampleCount),
                currPrm.sampleDataType, static_cast<uint32_t>(widthIn), static_cast<uint32_t>(heightIn),
                static_cast<int32_t>(strideIn), static_cast<uint32_t>(widthOut), static_cast<uint32_t>(heightOut),
                static_cast<int32_t>(strideOut), static_cast<uint8_t>(inputSampleRate),
                static_cast<uint8_t>(outputSampleRate), static_cast<uint8_t>(numChannels),
                static_cast<uint8_t>(isInterleave), static_cast<uint8_t>(bufferFormat), static_cast<uint8_t>(enableMMA),
                static_cast<uint32_t>(mmaSize), errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            TI_profile_stop();

            for (block = 0; block < numBlocks; block++) {
               if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                  if (bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) {
                     pInDBuffer = (float *) pIn.get();
                     pInStartOffset =
                         (block * currPrm.inputSampleCount * currPrm.numChannels) % inBufferCbElementsInterleaved;
                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData =
                            ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount * currPrm.numChannels);
                        copyRoi_float_circular((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                               inBufferCbSizeInterleaved, pInStartOffset,
                                               AUDIOLIB_sizeof(currPrm.sampleDataType));
                     }
                  }
                  else {
                     pInDBuffer = (float *) pIn.get();
                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData =
                            ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount * currPrm.numChannels);
                        copyRoi_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                      AUDIOLIB_sizeof(currPrm.sampleDataType));
                     }
                  }
               }
               else {
                  pInDBuffer = (float *) pIn.get();
                  if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                     pInData = ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount);
                     copyRoiDualStride_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                             currPrm.inputSampleCount * AUDIOLIB_sizeof(currPrm.sampleDataType) *
                                                 currPrm.numBlocks,
                                             AUDIOLIB_sizeof(currPrm.sampleDataType));
                  }
               }

               /* initialize set function parameters */
               TISP::SetAddr_t     kAddr;
               TISP::SetAddrMode_t kAddrMode;

               /* set address for block to process */
               kAddrMode    = TISP::kAddrModeInOut;
               kAddr.src[0] = (uint8_t *) pIn.get();
               kAddr.dst[0] = (uint8_t *) pOut.get() + block * heightOut * strideOut;
               k0->setAddr(kAddr, kAddrMode);

               TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT);
               TI_profile_start(TI_PROFILE_KERNEL_OPT);

               /* Execute the kernel */
               k0->exec();

               TI_profile_stop();
            }

            /* The following for loop is to call kernel repeatedly so as to
             * train the branch predictor                                   */
            for (k = 0; k < AUDIOLIB_SSRC_NUM_WARM_REPS; k++) {

               pInStartOffset = 0;

               TI_profile_start(TI_PROFILE_KERNEL_INIT);

               auto k0 = TISP_ssrc_createKernelObject(
                   pIn.get(), pState, pFiltCoeffs, pOut.get(), static_cast<uint32_t>(inputSampleCount),
                   currPrm.sampleDataType, static_cast<uint32_t>(widthIn), static_cast<uint32_t>(heightIn),
                   static_cast<int32_t>(strideIn), static_cast<uint32_t>(widthOut), static_cast<uint32_t>(heightOut),
                   static_cast<int32_t>(strideOut), static_cast<uint8_t>(inputSampleRate),
                   static_cast<uint8_t>(outputSampleRate), static_cast<uint8_t>(numChannels),
                   static_cast<uint8_t>(isInterleave), static_cast<uint8_t>(bufferFormat),
                   static_cast<uint8_t>(enableMMA), static_cast<uint32_t>(mmaSize), errorCtx);

               if (!errorCtx.isSuccess()) {
                  printf("\nError creating kernel: %s\n", errorCtx.getMessage());
                  return -1;
               }

               TI_profile_stop();

               for (block = 0; block < numBlocks; block++) {
                  if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                     if (bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) {
                        pInDBuffer = (float *) pIn.get();
                        pInStartOffset =
                            (block * currPrm.inputSampleCount * currPrm.numChannels) % inBufferCbElementsInterleaved;
                        if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                           pInData =
                               ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount * currPrm.numChannels);
                           copyRoi_float_circular((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                                  inBufferCbSizeInterleaved, pInStartOffset,
                                                  AUDIOLIB_sizeof(currPrm.sampleDataType));
                        }
                     }
                     else {
                        pInDBuffer = (float *) pIn.get();
                        if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                           pInData =
                               ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount * currPrm.numChannels);
                           copyRoi_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                         AUDIOLIB_sizeof(currPrm.sampleDataType));
                        }
                     }
                  }
                  else {
                     pInDBuffer = (float *) pIn.get();
                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData = ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount);
                        copyRoiDualStride_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                                currPrm.inputSampleCount * AUDIOLIB_sizeof(currPrm.sampleDataType) *
                                                    currPrm.numBlocks,
                                                AUDIOLIB_sizeof(currPrm.sampleDataType));
                     }
                  }

                  /* initialize set function parameters */
                  TISP::SetAddr_t     kAddr;
                  TISP::SetAddrMode_t kAddrMode;

                  /* set address for block to process */
                  kAddrMode    = TISP::kAddrModeInOut;
                  kAddr.src[0] = (uint8_t *) pIn.get();
                  kAddr.dst[0] = (uint8_t *) pOut.get() + block * heightOut * strideOut;
                  k0->setAddr(kAddr, kAddrMode);

                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

                  /* Execute the kernel */
                  k0->exec();

                  TI_profile_stop();
               }
            }

            /* get output to L1D */
            int16_t outSum   = 0;
            int8_t *pOutTemp = (int8_t *) pOut.get(); /* treat output as bytes to be data type agnostic */
            for (k = 0; k < widthOut; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            /* dummy store of outSum to insure that the compiler does not remove it. */
            volatileSum = outSum;

            /* Dummy loop for initializing the kernel object */
            for (size_t i = 0; i < 1; i++) {

               totalOutputSampleCnt = 0;

               pInStartOffset = 0;

               TI_profile_start(TI_PROFILE_KERNEL_INIT);

               auto k0 = TISP_ssrc_createKernelObject(
                   pIn.get(), pState, pFiltCoeffs, pOut.get(), static_cast<uint32_t>(inputSampleCount),
                   currPrm.sampleDataType, static_cast<uint32_t>(widthIn), static_cast<uint32_t>(heightIn),
                   static_cast<int32_t>(strideIn), static_cast<uint32_t>(widthOut), static_cast<uint32_t>(heightOut),
                   static_cast<int32_t>(strideOut), static_cast<uint8_t>(inputSampleRate),
                   static_cast<uint8_t>(outputSampleRate), static_cast<uint8_t>(numChannels),
                   static_cast<uint8_t>(isInterleave), static_cast<uint8_t>(bufferFormat),
                   static_cast<uint8_t>(enableMMA), static_cast<uint32_t>(mmaSize), errorCtx);

               if (!errorCtx.isSuccess()) {
                  printf("\nError creating kernel: %s\n", errorCtx.getMessage());
                  return -1;
               }

               TI_profile_stop();

               for (block = 0; block < numBlocks; block++) {
                  if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                     if (bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) {
                        pInDBuffer = (float *) pIn.get();
                        pInStartOffset =
                            (block * currPrm.inputSampleCount * currPrm.numChannels) % inBufferCbElementsInterleaved;
                        if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                           pInData =
                               ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount * currPrm.numChannels);
                           copyRoi_float_circular((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                                  inBufferCbSizeInterleaved, pInStartOffset,
                                                  AUDIOLIB_sizeof(currPrm.sampleDataType));
                        }
                     }
                     else {
                        pInDBuffer = (float *) pIn.get();
                        if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                           pInData =
                               ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount * currPrm.numChannels);
                           copyRoi_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                         AUDIOLIB_sizeof(currPrm.sampleDataType));
                        }
                     }
                  }
                  else {
                     pInDBuffer = (float *) pIn.get();
                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData = ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount);
                        copyRoiDualStride_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                                currPrm.inputSampleCount * AUDIOLIB_sizeof(currPrm.sampleDataType) *
                                                    currPrm.numBlocks,
                                                AUDIOLIB_sizeof(currPrm.sampleDataType));
                     }
                  }

                  /* initialize set function parameters */
                  TISP::SetAddr_t     kAddr;
                  TISP::SetAddrMode_t kAddrMode;

                  /* set address for block to process */
                  kAddrMode    = TISP::kAddrModeInOut;
                  kAddr.src[0] = (uint8_t *) pIn.get();
                  kAddr.dst[0] = (uint8_t *) pOut.get() + block * heightOut * strideOut;
                  k0->setAddr(kAddr, kAddrMode);

                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARMWRB);
                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

                  /* Execute the kernel */
                  k0->exec();

                  TI_profile_stop();
               }
            }

            for (block = 0; block < numBlocks; block++) {
               if (currPrm.staticOut != nullptr) {
                  comparisonDone = 1;
                  if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                     pOutData = (float *) currPrm.staticOut + totalOutputSampleCnt * currPrm.numChannels;
                     status_ref_vs_opt =
                         TI_compare_mem_2D_float((void *) pOutData, pOut.get() + block * heightOut * strideOut, 0.001,
                                                 (double) powf(10, -6), currPrm.numChannels, outFrameLength,
                                                 currPrm.numChannels * AUDIOLIB_sizeof(currPrm.sampleDataType),
                                                 AUDIOLIB_sizeof(currPrm.sampleDataType));
                  }
                  else {
                     pOutData          = (float *) currPrm.staticOut + totalOutputSampleCnt;
                     status_ref_vs_opt = TI_compare_mem_2DDualStride_float(
                         (void *) pOutData, pOut.get() + block * heightOut * strideOut, 0.001, (double) powf(10, -6),
                         outFrameLength, heightOut,
                         outFrameLength * numBlocks * AUDIOLIB_sizeof(currPrm.sampleDataType), strideOut,
                         AUDIOLIB_sizeof(currPrm.sampleDataType));
                  }
                  if (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) {
                     currentTestFail = 1;
                  }
               }
               else {
                  /* Set to pass since it wasn't supposed to run. */
                  status_ref_vs_opt = TI_TEST_KERNEL_PASS;
               }
               totalOutputSampleCnt += outFrameLength;
            }

            inputSize = widthIn;

            if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED &&
                bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) {

               bool     isUpsampling     = AUDIOLIB_ssrc_isUpsampling(inputSampleRate, outputSampleRate);
               float    sampleRateRatiof = AUDIOLIB_ssrc_getSampleRateRatio(inputSampleRate, outputSampleRate);
               uint32_t sampleRateRatio  = (uint32_t) sampleRateRatiof;

               setStatus =
                   AUDIOLIB_ssrc_cn_set(pIn.get(), pState, (uint8_t) AUDIOLIB_SSRC_MODE_RESET, inputSampleRate,
                                        outputSampleRate, numChannels, ssrcStateParams.inBufferTotalDimX,
                                        sampleRateRatio, inputSampleCount, bufferFormat, currPrm.sampleDataType);

               if (setStatus == false) {
                  printf("setting parameter for NATC failed\n");
                  continue;
               }

               totalOutputSampleCnt = 0;
               pInStartOffset       = 0;

               for (block = 0; block < numBlocks; block++) {
                  if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                     pInDBuffer = (float *) pIn.get();
                     if (bufferFormat == AUDIOLIB_SSRC_BUFFER_FORMAT_PING_PONG_CIRCULAR) {
                        pInStartOffset =
                            (block * currPrm.inputSampleCount * currPrm.numChannels) % inBufferCbElementsInterleaved;
                        if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                           pInData =
                               ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount * currPrm.numChannels);
                           copyRoi_float_circular((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                                  inBufferCbSizeInterleaved, pInStartOffset,
                                                  AUDIOLIB_sizeof(currPrm.sampleDataType));
                        }
                     }
                     else {
                        if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                           pInData =
                               ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount * currPrm.numChannels);
                           copyRoi_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                         AUDIOLIB_sizeof(currPrm.sampleDataType));
                        }
                     }
                  }
                  else {
                     pInDBuffer = (float *) pIn.get();
                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData = ((float *) prm[tpi].staticIn) + (block * currPrm.inputSampleCount);
                        copyRoiDualStride_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                                currPrm.inputSampleCount * AUDIOLIB_sizeof(currPrm.sampleDataType) *
                                                    currPrm.numBlocks,
                                                AUDIOLIB_sizeof(currPrm.sampleDataType));
                     }
                  }

                  AUDIOLIB_ssrc_cn(pIn.get(), pFilter, pOutCn, widthIn, numChannels, cirBuffAddressMask,
                                   inputSampleCount, isUpsampling, sampleRateRatio, outputSampleCount,
                                   &ssrcStateParams);

                  if (currPrm.staticOut != nullptr) {
                     pOutData          = (float *) currPrm.staticOut + totalOutputSampleCnt * currPrm.numChannels;
                     status_ref_vs_nat = TI_compare_mem_2D_float(
                         (void *) pOutData, pOutCn, 0.001, (double) powf(10, -6), currPrm.numChannels, outFrameLength,
                         currPrm.numChannels * AUDIOLIB_sizeof(currPrm.sampleDataType),
                         AUDIOLIB_sizeof(currPrm.sampleDataType));
                  }
                  else {
                     /* Set to pass since it wasn't supposed to run. */
                     status_ref_vs_nat = TI_TEST_KERNEL_PASS;
                  }
                  totalOutputSampleCnt += outFrameLength;
               }
            }
            else {
               status_ref_vs_nat = TI_TEST_KERNEL_PASS;
               comparisonDone    = 1;
            }
            /* Set the 'fail' flag based on test vector comparison results */
            currentTestFail =
                ((status_ref_vs_nat == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) ||
                 (comparisonDone == 0) || (currentTestFail == 1))
                    ? 1
                    : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
            pProfile[3 * tpi + 1] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
            pProfile[3 * tpi + 2] = (int32_t) TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);

            sprintf(desc, "%s generated input | Data size=%d ", testPatternString,
                    AUDIOLIB_sizeof(currPrm.sampleDataType));

            /* write to CSV, must happen prior to write to screen because
            TI_profile_formula_add clears values in counters */
            fprintf(fpOutputCSV,
                    "TISP_ssrc, %d, %d,  "
                    "%ld, %lu, %d, %.2f, %d\n",
                    testNum, currPrm.testPattern, widthIn * heightIn, archCycles, pProfile[3 * tpi + 1],
                    ((float) cycles[TI_PROFILE_KERNEL_OPT]) / ((float) archCycles), !currentTestFail);

            TI_profile_add_test(testNum++, currPrm.inputSampleCount * currPrm.numChannels, 0, 0, currentTestFail, desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", AUDIOLIB_sizeof(currPrm.sampleDataType),
                    currPrm.inputSampleCount, currPrm.numChannels);
            TI_profile_skip_test(desc);
            /* clear the counters between runs; normally handled by
            TI_profile_add_test */
            TI_profile_clear_run_stats();
         } /* end of memory allocation successful? */

         if (pState)
            TI_align_free(pState);
         myL2Heap.deallocate(pIn.get());
         myL2Heap.deallocate(pOut.get());
         TI_align_free(pFiltCoeffs);
      } /* end repetitions */
   }    /* end idat test cases */

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("_HOST_BUILD is defined.\n");
#endif
   return TISP_ssrc_d(&pProfile[0], 0);
}

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
   return 0;
}

#ifdef __cplusplus
}
#endif
