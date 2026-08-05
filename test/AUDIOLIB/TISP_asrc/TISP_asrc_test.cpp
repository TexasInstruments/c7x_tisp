// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// include AUDIOLIB
#include <audiolib.h>

#include "../common/TISP_unit_test_l2_heap.hpp"
#include "TISP_test.h"

#include <TISP_ErrorCtxt.hpp>
#include <tisp.hpp>

// include test data for this kernel
#include "TISP_asrc_cn.h"
#include "TISP_asrc_idat.h"
#include "common/AUDIOLIB_bufParams.h"

#if SOC == j722s
#define L2SRAM_HEAP_SIZE (1024 * 1024) // 1 MB
#elif SOC == AM62A || AM62D
#define L2SRAM_HEAP_SIZE (800 * 1024) // 800 KB
#elif SOC == AM275
#define L2SRAM_HEAP_SIZE (2048 * 1024) // 2MB KB
#endif

#ifdef WIN32
#if defined(__C7504__)
int8_t      l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t      ddrBuffer[2048 * 1024];
const float audiolib_asrc_h32_32kHz_to_32kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_32kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_32kHz_to_48kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_44_1kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_44_1kHz_to_48kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_48kHz_to_48kHz[] = {
#include "filt_coeffs/h32_48kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_44_1kHz_to_32kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_32kHz_rfmt.h"
};
const float audiolib_asrc_h32_48kHz_to_32kHz[] = {
#include "filt_coeffs/h32_48kHz_to_32kHz_rfmt.h"
};
const float audiolib_asrc_h32_48kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_48kHz_to_44_1kHz_rfmt.h"
};
#elif defined(__C7524__)
int8_t      l2auxBuffer[TISP_L2_BUFFER_SIZE];
int8_t      ddrBuffer[1 * 1024 * 1024];
const float audiolib_asrc_h32_32kHz_to_32kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_32kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_32kHz_to_48kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_44_1kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_44_1kHz_to_48kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_48kHz_to_48kHz[] = {
#include "filt_coeffs/h32_48kHz_to_48kHz_rfmt.h"
};
const float audiolib_asrc_h32_44_1kHz_to_32kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_32kHz_rfmt.h"
};
const float audiolib_asrc_h32_48kHz_to_32kHz[] = {
#include "filt_coeffs/h32_48kHz_to_32kHz_rfmt.h"
};
const float audiolib_asrc_h32_48kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_48kHz_to_44_1kHz_rfmt.h"
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
const float audiolib_asrc_h32_32kHz_to_32kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_32kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_32kHz_to_48kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_44_1kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_44_1kHz_to_48kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_48kHz_to_48kHz[] = {
#include "filt_coeffs/h32_48kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_44_1kHz_to_32kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_32kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_48kHz_to_32kHz[] = {
#include "filt_coeffs/h32_48kHz_to_32kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_48kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_48kHz_to_44_1kHz_rfmt.h"
};

#elif defined(__C7524__)
__attribute__((section(".ddrData"), aligned(128))) int8_t    ddrBuffer[1 * 1024 * 1024];
__attribute__((section(".l2sramData"), aligned(128))) int8_t l2SramHeap[L2SRAM_HEAP_SIZE];

__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_32kHz_to_32kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_32kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_32kHz_to_48kHz[] = {
#include "filt_coeffs/h32_32kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_44_1kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_44_1kHz_to_48kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_48kHz_to_48kHz[] = {
#include "filt_coeffs/h32_48kHz_to_48kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_44_1kHz_to_32kHz[] = {
#include "filt_coeffs/h32_44_1kHz_to_32kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_48kHz_to_32kHz[] = {
#include "filt_coeffs/h32_48kHz_to_32kHz_rfmt.h"
};
__attribute__((section(".coeffMemory"), aligned(AUDIOLIB_L2DATA_ALIGNMENT)))
const float audiolib_asrc_h32_48kHz_to_44_1kHz[] = {
#include "filt_coeffs/h32_48kHz_to_44_1kHz_rfmt.h"
};

#else
__attribute__((section(".msmcData"), aligned(64))) int8_t msmcBuffer[TISP_L3_RESULTS_BUFFER_SIZE];
__attribute__((section(".ddrData"), aligned(64))) int8_t  ddrBuffer[2048 * 1024];
#endif
#endif // WIN32

#define MATRIX_TRANSPOSE_ROW_STRIDE(x, y) (((x + y - 1) / y) * y)

/* Generate output file if defined! This is used for snr analysis*/
// #define ENABLE_GENERATE_OUTPUT_FILE
/* Parameter used to allocate the memory for a quadruple buffer for input data. User should not change this value as the
 * algorithm will not work properly if this value is changed.
   Application needs to allocate a quadruple buffer if the freme size is greater than 64 (_FSG64)*/
#define AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSG64 (4U)
/* Parameter used to allocate the memory for a double buffer for input data. User should not change this value as the
 * algorithm will not work properly if this value is changed.
   Application needs to allocate a double buffer if the freme size is less than 64 (_FSG64)*/
#define AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSL64 (2U)
/* Number of warm reps */
#define AUDIOLIB_ASRC_NUM_WARM_REPS (3U)
/* Inverleaved or noninterleaved data */
#define AUDIOLIB_ASRC_IS_INTERLEAVED_DATA (1U)

int16_t volatile volatileSum = 0; // use volatile to keep compiler from removing this operation

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
      sprintf(filename, "asrc_testcase%d_input.csv", testId);
   }
   else {
      sprintf(filename, "asrc_testcase%d_output.csv", testId);
   }

   if (blockNum == 0) {
      // First block - create new file
      fp = fopen(filename, "w");
   }
   else {
      // Append to existing file
      fp = fopen(filename, "a");
   }

   if (fp) {

      // Always write in interleaved format to the file
      for (s = 0; s < sampleCount; s++) {
         for (c = 0; c < numChannels; c++) {
            if (isInterleaved) {
               // For interleaved data: [ch0,ch1,ch0,ch1,...]
               fprintf(fp, "%.21g", pOut[s * numChannels + c]);
            }
            else {
               // For non-interleaved data: [ch0,ch0,...,ch1,ch1,...]
               fprintf(fp, "%.21g", pOut[c * sampleCount + s]);
            }
            // Print a comma only if it's not the last column
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

int TISP_asrc_d(uint32_t *pProfile, uint8_t LevelOfFeedback)
{
   int32_t  tpi;
   int32_t  currentTestFail;
   int32_t  fail = 0;
   uint32_t repCount;
   uint32_t numReps;

   uint32_t k = 0;
   uint32_t testNum;
   uint64_t archCycles = 0;
   uint64_t estCycles  = 0;

   int32_t  nonInterleavedDataBufSize, filtCoeffSizeBytes, filterRembufSize, outFrameLength;
   uint32_t filterLength;
   uint32_t inputSampleCount, filterRembufWidthIn, inBufferTotaDimx;
   float   *pInData, *pOutData;
   float   *pInDBuffer;

   int64_t t_asrc_cycles_opt, t_asrc_cycles_warm, t_asrc_cycles_warmwrb;
   int64_t asrc_cycles;
   float   processorUtilization;

   int32_t block;
   int32_t totalOutputSampleCnt, totalInputSampleCnt;

   TISP_asrc_testParams_t *prm;
   TISP_asrc_testParams_t  currPrm;
   TISP_asrc_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_asrc");

   /* file IO for benchmarking  */
   FILE *fpOutputCSV = fopen("TISP_asrc.csv", "w");
   fprintf(fpOutputCSV,
           "Kernel, Test ID, Test type, Bit Width, Parameters, "
           "Input Data Format, Input Sample Count, Blocks, Input Signal Frequency (Hz), Input Sample Rate (Hz), Output "
           "Sample Rate (Hz), Num of Channels, Output Sample Count, Processor Utilization (MHz), "
           "Arch cycles, Estimated cycles, Pass/Fail, Cold Cycles, Warm Cycles, Warm Cycles WRB\n");

   /*creates custom heap object */
   UnitTestHeap myL2Heap(&l2SramHeap[0], L2SRAM_HEAP_SIZE);

   for (tpi = 0; tpi < test_cases; tpi++) {
      myL2Heap.reset();
      numReps = prm[tpi].numReps;
      testNum = prm[tpi].testID;
      currPrm = prm[tpi];

      TISP::AUDIOLIB::TISP_asrcMode asrc_mode_ratio;

      for (repCount = 0; repCount < numReps; repCount++) {

         int32_t status_ref_vs_opt = TI_TEST_KERNEL_FAIL;
         int32_t status_ref_vs_nat = TI_TEST_KERNEL_FAIL;

         /* Sometimes, depending on certain compile flags, the test will be
          * marked PASS even if no comparison is done. This flag is to detect
          * if a comparison was done or not                                   */

         float fsin, fsout, sigFreq;

         fsout                = (float) convertSampleRateToInt((sample_rate_t) currPrm.outputSampleRate);
         fsin                 = (float) convertSampleRateToInt((sample_rate_t) currPrm.inputSampleRate);
         sigFreq              = (float) currPrm.signalFrequency;
         processorUtilization = 0.0;
         void *pOutCn;

         int32_t inputSize, pInAlign;

         int32_t ref_vs_opt_comparisonDone = 0;
         int32_t ref_vs_nat_comparisonDone = 0;
         t_asrc_cycles_opt                 = 0;
         t_asrc_cycles_warm                = 0;
         t_asrc_cycles_warmwrb             = 0;
         currentTestFail                   = 0;
         int32_t eleCount                  = 0;
         bool    setStatus                 = true;

         size_t               inDataSize             = AUDIOLIB_sizeof(currPrm.sampleDataType);
         size_t               maxSampleCountPerBlock = currPrm.maxSampleCountPerBlock;
         size_t               heightIn               = currPrm.heightIn;
         size_t               numChannels            = currPrm.heightIn;
         sample_rate_t        inputSampleRate        = (sample_rate_t) currPrm.inputSampleRate;
         sample_rate_t        outputSampleRate       = (sample_rate_t) currPrm.outputSampleRate;
         size_t               numBlocks              = currPrm.numBlocks;
         size_t               isInterleave           = currPrm.dataFormat;
         size_t               frameModuloFactor      = currPrm.moduloFactor;
         size_t               widthIn, widthOut, heightOut, strideIn, strideOut;
         uint32_t             alignment_size;
         TISP_asrcStateParams asrcStateParams;

         filtCoeffSizeBytes = AUDIOLIB_asrc_getFilterCoeffSize(currPrm.sampleDataType);
         filterRembufSize =
             AUDIOLIB_asrc_getFilterRembufSize(currPrm.heightIn, currPrm.sampleDataType, frameModuloFactor);
         outFrameLength = AUDIOLIB_asrc_getOutBufferLength(inputSampleRate, outputSampleRate, maxSampleCountPerBlock);
         filterLength   = AUDIOLIB_asrc_getFilterLength();

         if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
            /* Matrix transpose implementation make use of two SE engine to fetch
               two full vector in each iteration extra rows are padded to ensure SE access
               to allocated memory regions */
            if ((inDataSize == 4) || (inDataSize == 8))
               eleCount = 2 * (__C7X_VEC_SIZE_BYTES__ / inDataSize);
            else
               eleCount = __C7X_VEC_SIZE_BYTES__ / inDataSize;
            int32_t dim_y_padded = MATRIX_TRANSPOSE_ROW_STRIDE(maxSampleCountPerBlock, eleCount);

            heightIn = maxSampleCountPerBlock;
            strideIn = currPrm.heightIn * inDataSize;
            widthIn  = currPrm.heightIn;

            heightOut = outFrameLength;
            strideOut = currPrm.heightIn * inDataSize;
            widthOut  = currPrm.heightIn;

            filterRembufWidthIn = (uint32_t) numChannels;

            if (maxSampleCountPerBlock >= filterLength) {
               alignment_size = maxSampleCountPerBlock * AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSL64 * inDataSize;
            }
            else {
               alignment_size = filterLength * AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSL64 * inDataSize;
            }

            nonInterleavedDataBufSize = AUDIOLIB_asrc_getNonInterleavedDataBufSize(
                currPrm.heightIn, maxSampleCountPerBlock, currPrm.sampleDataType, currPrm.dataFormat);
            inputSize = strideIn * dim_y_padded;
            pInAlign  = 64;
         }
         else {
            heightIn = currPrm.heightIn;
            if (maxSampleCountPerBlock >= filterLength) {
               strideIn = maxSampleCountPerBlock * AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSG64 * inDataSize;
            }
            else {
               strideIn = filterLength * AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSL64 * inDataSize;
            }
            widthIn = maxSampleCountPerBlock;

            heightOut = currPrm.heightIn;
            strideOut = outFrameLength * inDataSize;
            widthOut  = outFrameLength;

            filterRembufWidthIn = (uint32_t) (frameModuloFactor - 1);

            inputSize = strideIn * heightIn;
            pInAlign  = strideIn;

            nonInterleavedDataBufSize = 0;
            alignment_size            = 0;
         }

         auto pFiltCoeffs   = myL2Heap.allocate64ByteAligned<uint8_t>(filtCoeffSizeBytes);
         auto pFilterRembuf = myL2Heap.allocate64ByteAligned<uint8_t>(filterRembufSize);

         auto pOut = myL2Heap.allocate64ByteAligned<uint8_t>(strideOut * heightOut);
         memset((uint8_t *) pOut.get(), 0, strideOut * heightOut);

         auto pIn                 = (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED)
                                        ? myL2Heap.allocate64ByteAligned<uint8_t>(inputSize)
                                        : myL2Heap.allocateCustomAligned<uint8_t>(inputSize, pInAlign);
         auto pNonInterleavedData = myL2Heap.allocateCustomAligned<uint8_t>(nonInterleavedDataBufSize, alignment_size);

         memset((uint8_t *) pFiltCoeffs.get(), 0, filtCoeffSizeBytes);
         memset((uint8_t *) pFilterRembuf.get(), 0, filterRembufSize);
         memset((uint8_t *) pNonInterleavedData.get(), 0, nonInterleavedDataBufSize);
         memset((uint8_t *) pIn.get(), 0, inputSize);

         size_t strideInElements  = strideIn / inDataSize;
         size_t strideOutElements = strideOut / inDataSize;

         switch (inputSampleRate) {
         case SAMPLE_RATE_32000: {
            if (outputSampleRate == SAMPLE_RATE_32000) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_32kHz_to_32kHz, filtCoeffSizeBytes);
            }
            else if (outputSampleRate == SAMPLE_RATE_44100) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_32kHz_to_44_1kHz, filtCoeffSizeBytes);
            }
            else if (outputSampleRate == SAMPLE_RATE_48000) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_32kHz_to_48kHz, filtCoeffSizeBytes);
            }
            break;
         }
         case SAMPLE_RATE_44100: {
            if (outputSampleRate == SAMPLE_RATE_32000) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_44_1kHz_to_32kHz, filtCoeffSizeBytes);
            }
            else if (outputSampleRate == SAMPLE_RATE_44100) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_44_1kHz_to_44_1kHz, filtCoeffSizeBytes);
            }
            else if (outputSampleRate == SAMPLE_RATE_48000) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_44_1kHz_to_48kHz, filtCoeffSizeBytes);
            }
            break;
         }
         case SAMPLE_RATE_48000: {
            if (outputSampleRate == SAMPLE_RATE_32000) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_48kHz_to_32kHz, filtCoeffSizeBytes);
            }
            else if (outputSampleRate == SAMPLE_RATE_44100) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_48kHz_to_44_1kHz, filtCoeffSizeBytes);
            }
            else if (outputSampleRate == SAMPLE_RATE_48000) {
               memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_48kHz_to_48kHz, filtCoeffSizeBytes);
            }
            break;
         }
         default: {
            memcpy(pFiltCoeffs.get(), audiolib_asrc_h32_48kHz_to_48kHz, filtCoeffSizeBytes);
            break;
         }
         }

         pOutCn = (void *) ddrBuffer;
         memset(pOutCn, 0, strideOut * heightOut);

         /* Only run the test if the buffer allocations fit in the heap */
         if (pIn.get() && pOut.get() && pOutCn && pFiltCoeffs.get() && pFilterRembuf.get() &&
             ((pNonInterleavedData.get() == nullptr && isInterleave != AUDIOLIB_DATA_FORMAT_INTERLEAVED) ||
              (pNonInterleavedData.get() != nullptr && isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED))) {

            // C7000 cold run
            t_asrc_cycles_opt = 0;

            TI_profile_start(TI_PROFILE_KERNEL_INIT);

            TISP::ErrorCtxt errorCtx;

            auto k0 = std::make_unique<TISP::AUDIOLIB::Asrc<float>>(
                (float *) pIn.get(), (float *) pNonInterleavedData.get(), (float *) pFiltCoeffs.get(),
                (float *) pFilterRembuf.get(), (float *) pOut.get(), static_cast<uint32_t>(widthIn),
                static_cast<uint32_t>(heightIn), static_cast<int32_t>(strideIn), static_cast<uint32_t>(widthOut),
                static_cast<uint32_t>(heightOut), static_cast<int32_t>(strideOut),
                static_cast<uint32_t>(maxSampleCountPerBlock), static_cast<uint32_t>(asrcStateParams.outputSampleCount),
                currPrm.sampleDataType, static_cast<uint32_t>(inputSampleRate), static_cast<uint32_t>(outputSampleRate),
                static_cast<uint32_t>(numChannels), static_cast<uint32_t>(frameModuloFactor),
                static_cast<uint32_t>(isInterleave), static_cast<int32_t>(nonInterleavedDataBufSize), errorCtx);

            if (!errorCtx.isSuccess()) {
               printf("\nError creating kernel: %s\n", errorCtx.getMessage());
               return -1;
            }

            TI_profile_stop();

            asrc_mode_ratio.mode      = (uint8_t) AUDIOLIB_ASRC_MODE_RESET;
            asrc_mode_ratio.asrcRatio = fsout / fsin;
            k0->setParams(static_cast<uint16_t>(TISP::AUDIOLIB::AsrcParamId::kMode), &asrc_mode_ratio,
                          sizeof(asrc_mode_ratio), errorCtx);
            if (!errorCtx.isSuccess()) {
               printf("\nError setting parameters: %s\n", errorCtx.getMessage());
               errorCtx.reset();
            }

            totalInputSampleCnt               = 0;
            asrcStateParams.outputSampleCount = 0;
            totalOutputSampleCnt              = 0;
            for (block = 0; block < (int32_t) numBlocks; block++) {
               inputSampleCount = (int32_t) maxSampleCountPerBlock;
               if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                  pInDBuffer = (float *) pIn.get();

                  if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                     pInData = ((float *) prm[tpi].staticIn0) + (totalInputSampleCnt * currPrm.heightIn);
                     copyRoi_float((void *) pInDBuffer, (void *) pInData, currPrm.heightIn, inputSampleCount,
                                   currPrm.heightIn * inDataSize, inDataSize);
                  }
               }
               else {
                  if (maxSampleCountPerBlock >= filterLength) {
                     pInDBuffer = (float *) pIn.get() + (block % AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSG64) * widthIn;
                  }
                  else {
                     pInDBuffer =
                         (float *) pIn.get() +
                         (block % ((filterLength / widthIn) * AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSL64)) * widthIn;
                  }
                  if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                     pInData = ((float *) prm[tpi].staticIn0) + totalInputSampleCnt;
                     copyRoiDualStride_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                             maxSampleCountPerBlock * inDataSize * numBlocks, inDataSize);
                  }
               }

               asrc_mode_ratio.mode      = (uint8_t) AUDIOLIB_ASRC_MODE_SET;
               asrc_mode_ratio.asrcRatio = fsout / fsin;
               k0->setParams(static_cast<uint16_t>(TISP::AUDIOLIB::AsrcParamId::kMode), &asrc_mode_ratio,
                             sizeof(asrc_mode_ratio), errorCtx);
               if (!errorCtx.isSuccess()) {
                  printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                  errorCtx.reset();
               }

               /* initialize set function parameters */
               TISP::SetAddr_t     kAddr;
               TISP::SetAddrMode_t kAddrMode;

               /* set address for block to process */
               kAddrMode = TISP::kAddrModeInOut;

               kAddr.src[0] = (uint8_t *) pIn.get();
               kAddr.dst[0] = (uint8_t *) pOut.get();
               k0->setAddr(kAddr, kAddrMode);

               /* run warm instruction cache test */
               TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT);
               TI_profile_start(TI_PROFILE_KERNEL_OPT);

               /* Execute the kernel */
               k0->exec();

               TI_profile_stop();

               asrcStateParams.outputSampleCount = k0->getNumOutputSampleCount();
               asrc_cycles                       = TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
               t_asrc_cycles_opt += asrc_cycles;

#ifdef ENABLE_GENERATE_OUTPUT_FILE
               if (repCount == 0) { // Only for cold run
                  int32_t isInterleaved = (int32_t) AUDIOLIB_DATA_FORMAT_INTERLEAVED;
                  writeInOutDataToFile(pInDBuffer, currPrm.heightIn, currPrm.maxSampleCountPerBlock, currPrm.testID,
                                       block, isInterleaved, 1);
                  writeInOutDataToFile((float *) pOut, currPrm.heightIn, asrcStateParams.outputSampleCount,
                                       currPrm.testID, block, isInterleaved, 0);
               }
#endif
            }

            for (k = 0; k < NUM_REPS; k++) {

               t_asrc_cycles_warm                = 0;
               asrcStateParams.outputSampleCount = 0;

               TI_profile_start(TI_PROFILE_KERNEL_INIT);

               auto k0 = std::make_unique<TISP::AUDIOLIB::Asrc<float>>(
                   (float *) pIn.get(), (float *) pNonInterleavedData.get(), (float *) pFiltCoeffs.get(),
                   (float *) pFilterRembuf.get(), (float *) pOut.get(), static_cast<uint32_t>(widthIn),
                   static_cast<uint32_t>(heightIn), static_cast<int32_t>(strideIn), static_cast<uint32_t>(widthOut),
                   static_cast<uint32_t>(heightOut), static_cast<int32_t>(strideOut),
                   static_cast<uint32_t>(maxSampleCountPerBlock),
                   static_cast<uint32_t>(asrcStateParams.outputSampleCount), currPrm.sampleDataType,
                   static_cast<uint32_t>(inputSampleRate), static_cast<uint32_t>(outputSampleRate),
                   static_cast<uint32_t>(numChannels), static_cast<uint32_t>(frameModuloFactor),
                   static_cast<uint32_t>(isInterleave), static_cast<int32_t>(nonInterleavedDataBufSize), errorCtx);

               if (!errorCtx.isSuccess()) {
                  printf("\nError creating kernel: %s\n", errorCtx.getMessage());
                  return -1;
               }

               TI_profile_stop();

               asrc_mode_ratio.mode      = (uint8_t) AUDIOLIB_ASRC_MODE_RESET;
               asrc_mode_ratio.asrcRatio = fsout / fsin;
               k0->setParams(static_cast<uint16_t>(TISP::AUDIOLIB::AsrcParamId::kMode), &asrc_mode_ratio,
                             sizeof(asrc_mode_ratio), errorCtx);
               if (!errorCtx.isSuccess()) {
                  printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                  errorCtx.reset();
               }

               totalInputSampleCnt               = 0;
               asrcStateParams.outputSampleCount = 0;
               totalOutputSampleCnt              = 0;
               for (block = 0; block < (int32_t) numBlocks; block++) {
                  inputSampleCount = (int32_t) maxSampleCountPerBlock;
                  if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                     pInDBuffer = (float *) pIn.get();

                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData = ((float *) prm[tpi].staticIn0) + (totalInputSampleCnt * currPrm.heightIn);
                        copyRoi_float((void *) pInDBuffer, (void *) pInData, currPrm.heightIn, inputSampleCount,
                                      currPrm.heightIn * inDataSize, inDataSize);
                     }
                  }
                  else {
                     if (maxSampleCountPerBlock >= filterLength) {
                        pInDBuffer =
                            (float *) pIn.get() + (block % AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSG64) * widthIn;
                     }
                     else {
                        pInDBuffer =
                            (float *) pIn.get() +
                            (block % ((filterLength / widthIn) * AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSL64)) * widthIn;
                     }
                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData = ((float *) prm[tpi].staticIn0) + totalInputSampleCnt;
                        copyRoiDualStride_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                                maxSampleCountPerBlock * inDataSize * numBlocks, inDataSize);
                     }
                  }

                  asrc_mode_ratio.mode      = (uint8_t) AUDIOLIB_ASRC_MODE_SET;
                  asrc_mode_ratio.asrcRatio = fsout / fsin;
                  k0->setParams(static_cast<uint16_t>(TISP::AUDIOLIB::AsrcParamId::kMode), &asrc_mode_ratio,
                                sizeof(asrc_mode_ratio), errorCtx);
                  if (!errorCtx.isSuccess()) {
                     printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                     errorCtx.reset();
                  }

                  /* initialize set function parameters */
                  TISP::SetAddr_t     kAddr;
                  TISP::SetAddrMode_t kAddrMode;

                  /* set address for block to process */
                  kAddrMode = TISP::kAddrModeInOut;

                  kAddr.src[0] = (uint8_t *) pIn.get();

                  kAddr.dst[0] = (uint8_t *) pOut.get();
                  k0->setAddr(kAddr, kAddrMode);

                  /* run warm instruction cache test */
                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);

                  // Execute the kernel
                  k0->exec();

                  TI_profile_stop();

                  asrcStateParams.outputSampleCount = k0->getNumOutputSampleCount();

                  asrc_cycles = TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);
                  t_asrc_cycles_warm += asrc_cycles;
               }
            }

            /* initialize the kernel to use the c7000 optimized kernel */
            // WARMWRB run

            // get output to L1D
            int16_t outSum   = 0;
            int8_t *pOutTemp = (int8_t *) pOut.get(); // treat output as bytes to be data type agnostic
            for (k = 0; k < widthIn; k++) {
               outSum += *pOutTemp;
               pOutTemp++;
            }

            /* dummy store of outSum to insure that the compiler does not remove it. */
            volatileSum = outSum;

            for (block = 0; block < 1; block++) {

               totalInputSampleCnt               = 0;
               asrcStateParams.outputSampleCount = 0;
               totalOutputSampleCnt              = 0;
               TI_profile_start(TI_PROFILE_KERNEL_INIT);

               auto k0 = std::make_unique<TISP::AUDIOLIB::Asrc<float>>(
                   (float *) pIn.get(), (float *) pNonInterleavedData.get(), (float *) pFiltCoeffs.get(),
                   (float *) pFilterRembuf.get(), (float *) pOut.get(), static_cast<uint32_t>(widthIn),
                   static_cast<uint32_t>(heightIn), static_cast<int32_t>(strideIn), static_cast<uint32_t>(widthOut),
                   static_cast<uint32_t>(heightOut), static_cast<int32_t>(strideOut),
                   static_cast<uint32_t>(maxSampleCountPerBlock),
                   static_cast<uint32_t>(asrcStateParams.outputSampleCount), currPrm.sampleDataType,
                   static_cast<uint32_t>(inputSampleRate), static_cast<uint32_t>(outputSampleRate),
                   static_cast<uint32_t>(numChannels), static_cast<uint32_t>(frameModuloFactor),
                   static_cast<uint32_t>(isInterleave), static_cast<int32_t>(nonInterleavedDataBufSize), errorCtx);

               if (!errorCtx.isSuccess()) {
                  printf("\nError creating kernel: %s\n", errorCtx.getMessage());
                  return -1;
               }

               TI_profile_stop();

               asrc_mode_ratio.mode      = (uint8_t) AUDIOLIB_ASRC_MODE_RESET;
               asrc_mode_ratio.asrcRatio = fsout / fsin;
               k0->setParams(static_cast<uint16_t>(TISP::AUDIOLIB::AsrcParamId::kMode), &asrc_mode_ratio,
                             sizeof(asrc_mode_ratio), errorCtx);
               if (!errorCtx.isSuccess()) {
                  printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                  errorCtx.reset();
               }

               for (block = 0; block < (int32_t) numBlocks; block++) {
                  inputSampleCount = (int32_t) maxSampleCountPerBlock;
                  if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                     pInDBuffer = (float *) pIn.get();

                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData = ((float *) prm[tpi].staticIn0) + (totalInputSampleCnt * currPrm.heightIn);
                        copyRoi_float((void *) pInDBuffer, (void *) pInData, currPrm.heightIn, inputSampleCount,
                                      currPrm.heightIn * inDataSize, inDataSize);
                     }
                  }
                  else {
                     if (maxSampleCountPerBlock >= filterLength) {
                        pInDBuffer =
                            (float *) pIn.get() + (block % AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSG64) * widthIn;
                     }
                     else {
                        pInDBuffer =
                            (float *) pIn.get() +
                            (block % ((filterLength / widthIn) * AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSL64)) * widthIn;
                     }
                     if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                        pInData = ((float *) prm[tpi].staticIn0) + totalInputSampleCnt;
                        copyRoiDualStride_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                                maxSampleCountPerBlock * inDataSize * numBlocks, inDataSize);
                     }
                  }

                  asrc_mode_ratio.mode      = (uint8_t) AUDIOLIB_ASRC_MODE_SET;
                  asrc_mode_ratio.asrcRatio = fsout / fsin;
                  k0->setParams(static_cast<uint16_t>(TISP::AUDIOLIB::AsrcParamId::kMode), &asrc_mode_ratio,
                                sizeof(asrc_mode_ratio), errorCtx);
                  if (!errorCtx.isSuccess()) {
                     printf("\nError setting parameters: %s\n", errorCtx.getMessage());
                     errorCtx.reset();
                  }

                  /*initialize set function parameters */
                  TISP::SetAddr_t     kAddr;
                  TISP::SetAddrMode_t kAddrMode;

                  /* set address for block to process */
                  kAddrMode = TISP::kAddrModeInOut;

                  kAddr.src[0] = (uint8_t *) pIn.get();

                  kAddr.dst[0] = (uint8_t *) pOut.get();
                  k0->setAddr(kAddr, kAddrMode);

                  /* run warm instruction cache test */
                  TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARMWRB);
                  TI_profile_start(TI_PROFILE_KERNEL_OPT_WARMWRB);

                  // Execute the kernel
                  k0->exec();

                  TI_profile_stop();

                  asrcStateParams.outputSampleCount = k0->getNumOutputSampleCount();
                  asrc_cycles                       = TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARMWRB);
                  t_asrc_cycles_warmwrb += asrc_cycles;

                  if (currPrm.staticOut != nullptr) {

                     if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
                        pOutData = (float *) currPrm.staticOut + totalOutputSampleCnt * currPrm.heightIn;

                        status_ref_vs_opt =
                            TI_compare_mem_2D_float((void *) pOutData, pOut.get(), 0.001, (double) powf(10, -4),
                                                    currPrm.heightIn, asrcStateParams.outputSampleCount,
                                                    currPrm.heightIn * AUDIOLIB_sizeof(currPrm.sampleDataType),
                                                    AUDIOLIB_sizeof(currPrm.sampleDataType));
                     }
                     else {
                        pOutData          = (float *) currPrm.staticOut + totalOutputSampleCnt;
                        status_ref_vs_opt = TI_compare_mem_2DDualStride_float(
                            (void *) pOutData, pOut.get(), 0.001, (double) powf(10, -4),
                            asrcStateParams.outputSampleCount, heightOut,
                            currPrm.genOutSamplesPerChannel * AUDIOLIB_sizeof(currPrm.sampleDataType), strideOut,
                            AUDIOLIB_sizeof(currPrm.sampleDataType));
                     }

                     if (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) {
                        currentTestFail = 1;
                     }
                     ref_vs_opt_comparisonDone = 1;
                  }
                  else {
                     /* Set to pass since it wasn't supposed to run. */
                     status_ref_vs_opt = TI_TEST_KERNEL_PASS;
                  }
                  totalInputSampleCnt += inputSampleCount;
                  totalOutputSampleCnt += asrcStateParams.outputSampleCount;
               }
            }

            processorUtilization =
                (float) t_asrc_cycles_warm * fsin / (currPrm.maxSampleCountPerBlock * block * pow(10, 6));

            asrc_mode_ratio.asrcRatio = fsout / fsin;
            asrc_mode_ratio.mode      = (uint8_t) AUDIOLIB_ASRC_MODE_RESET;

            asrcStateParams.outputSampleCount = 0;

            setStatus = AUDIOLIB_asrc_cn_set(asrc_mode_ratio.mode, asrc_mode_ratio.asrcRatio, pNonInterleavedData.get(),
                                             pIn.get(), currPrm.sampleDataType, isInterleave, maxSampleCountPerBlock,
                                             currPrm.heightIn, &asrcStateParams);
            totalOutputSampleCnt = 0;
            totalInputSampleCnt  = 0;

            if (isInterleave == AUDIOLIB_DATA_FORMAT_INTERLEAVED) {
               if (maxSampleCountPerBlock >= AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS) {
                  inBufferTotaDimx = maxSampleCountPerBlock * AUDIOLIB_ASRC_DOUBLE_BUFFERING_FACTOR;
               }
               else {
                  inBufferTotaDimx = AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS * AUDIOLIB_ASRC_DOUBLE_BUFFERING_FACTOR;
               }
            }
            else {
               if (maxSampleCountPerBlock >= AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS) {
                  inBufferTotaDimx = maxSampleCountPerBlock * AUDIOLIB_ASRC_QUADRUPLE_BUFFERING_FACTOR;
               }
               else {
                  inBufferTotaDimx = AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS * AUDIOLIB_ASRC_DOUBLE_BUFFERING_FACTOR;
               }
            }

            asrcStateParams.cirBuffStartIdx =
                (int32_t) inBufferTotaDimx - (int32_t) (AUDIOLIB_ASRC_NUMBER_OF_SUB_FILTER_TAPS - 1);

            float sampleRateRatio =
                (float) convertSampleRateToInt(outputSampleRate) / (float) convertSampleRateToInt(inputSampleRate);
            double maxOutputsPerInputRatio =
                sampleRateRatio +
                ((float) AUDIOLIB_ASRC_MAX_MODULO_FACTOR - (float) frameModuloFactor) / maxSampleCountPerBlock;

            for (block = 0; block < (int32_t) numBlocks; block++) {

               inputSampleCount = (int32_t) currPrm.maxSampleCountPerBlock;

               if (isInterleave == 1) {
                  pInDBuffer = (float *) pIn.get();
                  if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                     pInData = ((float *) prm[tpi].staticIn0) + (totalInputSampleCnt * currPrm.heightIn);
                     copyRoi_float((void *) pInDBuffer, (void *) pInData, currPrm.heightIn, inputSampleCount,
                                   currPrm.heightIn * AUDIOLIB_sizeof(currPrm.sampleDataType),
                                   AUDIOLIB_sizeof(currPrm.sampleDataType));
                  }
               }
               else {
                  if (maxSampleCountPerBlock >= filterLength) {
                     pInDBuffer = (float *) pIn.get() + (block % AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSG64) * widthIn;
                  }
                  else {
                     pInDBuffer =
                         (float *) pIn.get() +
                         (block % ((filterLength / widthIn) * AUDIOLIB_ASRC_INPUT_BUFFER_MULTIPLE_FSL64)) * widthIn;
                  }
                  if (currPrm.sampleDataType == AUDIOLIB_FLOAT32) {
                     pInData = ((float *) prm[tpi].staticIn0) + totalInputSampleCnt;
                     copyRoiDualStride_float((void *) pInDBuffer, (void *) pInData, widthIn, heightIn, strideIn,
                                             maxSampleCountPerBlock * AUDIOLIB_sizeof(currPrm.sampleDataType) *
                                                 numBlocks,
                                             AUDIOLIB_sizeof(currPrm.sampleDataType));
                  }
               }

               asrc_mode_ratio.asrcRatio = fsout / fsin;
               asrc_mode_ratio.mode      = (uint8_t) AUDIOLIB_ASRC_MODE_SET;

               setStatus = AUDIOLIB_asrc_cn_set(asrc_mode_ratio.mode, asrc_mode_ratio.asrcRatio, nullptr, nullptr,
                                                currPrm.sampleDataType, isInterleave, maxSampleCountPerBlock,
                                                currPrm.heightIn, &asrcStateParams);
               if (setStatus == false) {
                  printf("setting parameter for NATC failed\n");
                  continue;
               }
               AUDIOLIB_asrc_cn(pIn.get(), pNonInterleavedData.get(), pFiltCoeffs.get(), pFilterRembuf.get(), pOutCn,
                                widthOut, currPrm.heightIn, &asrcStateParams, inputSampleCount, maxOutputsPerInputRatio,
                                strideInElements, strideOutElements, isInterleave, filterRembufWidthIn,
                                inBufferTotaDimx, frameModuloFactor, currPrm.sampleDataType);

               if (currPrm.staticOut != nullptr) {
                  if (isInterleave == 1) {
                     pOutData          = (float *) currPrm.staticOut + totalOutputSampleCnt * currPrm.heightIn;
                     status_ref_vs_nat = TI_compare_mem_2D_float(
                         (void *) pOutData, pOutCn, 0.001, (double) powf(10, -4), currPrm.heightIn,
                         asrcStateParams.outputSampleCount, currPrm.heightIn * AUDIOLIB_sizeof(currPrm.sampleDataType),
                         AUDIOLIB_sizeof(currPrm.sampleDataType));
                  }
                  else {
                     pOutData          = (float *) currPrm.staticOut + totalOutputSampleCnt;
                     status_ref_vs_nat = TI_compare_mem_2DDualStride_float(
                         (void *) pOutData, pOutCn, 0.001, (double) powf(10, -4), asrcStateParams.outputSampleCount,
                         heightOut, currPrm.genOutSamplesPerChannel * AUDIOLIB_sizeof(currPrm.sampleDataType),
                         strideOut, AUDIOLIB_sizeof(currPrm.sampleDataType));
                  }

                  if (status_ref_vs_nat == TI_TEST_KERNEL_FAIL) {
                     currentTestFail = 1;
                  }
                  ref_vs_nat_comparisonDone = 1;
               }
               else {
                  /* Set to pass since it wasn't supposed to run. */
                  status_ref_vs_nat = TI_TEST_KERNEL_PASS;
               }

               totalOutputSampleCnt += asrcStateParams.outputSampleCount;
               totalInputSampleCnt += inputSampleCount;
            }

            /* Set the 'fail' flag based on test vector comparison results */

            currentTestFail =
                ((status_ref_vs_nat == TI_TEST_KERNEL_FAIL) || (status_ref_vs_opt == TI_TEST_KERNEL_FAIL) ||
                 (ref_vs_opt_comparisonDone == 0) || (ref_vs_nat_comparisonDone == 0) || (currentTestFail == 1) ||
                 (currPrm.testPattern != STATIC))
                    ? 1
                    : 0;

            fail = ((fail == 1) || (currentTestFail == 1)) ? 1 : 0;

            pProfile[3 * tpi]     = t_asrc_cycles_opt / currPrm.numBlocks;
            pProfile[3 * tpi + 1] = t_asrc_cycles_warm / currPrm.numBlocks;
            pProfile[3 * tpi + 2] = t_asrc_cycles_warmwrb / currPrm.numBlocks;

            sprintf(desc,
                    "STATIC generated input | %s, Input Sample Count=%d, Blocks=%d, Input Sample Rate=%u Hz, Output "
                    "Sample Rate=%u Hz, Signal Frequency=%0.3f kHz, Num of Channels=%d",
                    isInterleave == 1 ? "Interleaved" : "Non-Interleaved", currPrm.maxSampleCountPerBlock, block,
                    (uint32_t) fsin, (uint32_t) fsout, (sigFreq / 1000), currPrm.heightIn);

            /* write to CSV, must happen prior to write to screen because
             TI_profile_formula_add clears values in counters */
            fprintf(fpOutputCSV,
                    "ASRC, %d, %d, %d, Input Data Format = %s Input Sample Count=%d Blocks=%d Input Signal "
                    "Frequency=%0.3f Input Sample "
                    "Rate=%u Output Sample Rate=%u Num of Channels=%d, "
                    "%s, %u, %d, %0.3f, %u, %u, %d, %d, %5.2f, %lu, %lu, %d, %d, %d, %d\n",
                    testNum, currPrm.testPattern, AUDIOLIB_sizeof(currPrm.sampleDataType) * 8,
                    (isInterleave == 1) ? "Interleaved" : "Non-Interleaved", currPrm.maxSampleCountPerBlock, block,
                    sigFreq, (uint32_t) fsin, (uint32_t) fsout, currPrm.heightIn,
                    (isInterleave == 1) ? "Interleaved" : "Non-Interleaved", currPrm.maxSampleCountPerBlock, block,
                    sigFreq, (uint32_t) fsin, (uint32_t) fsout, currPrm.heightIn, totalOutputSampleCnt,
                    processorUtilization, archCycles, estCycles, !currentTestFail, pProfile[3 * tpi],
                    pProfile[3 * tpi + 1], pProfile[3 * tpi + 2]);

            TI_profile_add_test(testNum++, currPrm.maxSampleCountPerBlock * currPrm.heightIn, 0, 0, currentTestFail,
                                desc);
         TISP::handleAllocator().reset();
         TISP::nodeAllocator().reset();
         }
         else {
            sprintf(desc, "Data size=%d, widthIn size=%d, heightIn size=%d", AUDIOLIB_sizeof(currPrm.sampleDataType),
                    currPrm.maxSampleCountPerBlock, currPrm.heightIn);
            TI_profile_skip_test(desc);
            // clear the counters between runs; normally handled by
            // TI_profile_add_test
            TI_profile_clear_run_stats();
         } // end of memory allocation successful?
      }    // end repetitions
   }       // end idat test cases

   /* Close results CSV */
   fclose(fpOutputCSV);

   return fail;
}

int test_main(uint32_t *pProfile)
{
#if !defined(_HOST_BUILD)
   printf("_HOST_BUILD is defined.\n");
#endif
   return TISP_asrc_d(&pProfile[0], 0);
}

/* Main call for individual test projects */
#ifdef __cplusplus
extern "C" {
#endif
int test_main_d()
{
   int fail = 1;

   uint32_t profile[512 * 3];

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
