// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// -----------------------------------------------------------------------
//  TwiddleLUT Signal Chain: Multi-Rate Subgraph Test
//
//  Models a multi-rate audio system with up to 4 subgraphs (threads),
//  each at a potentially different FFT size, using batched FFT nodes.
//
//  Graph (single opVec, no LUT reset between subgraphs):
//    SG0: Gain(N0, ch0) -> RFFT(N0, ch0) -> IFFT(N0, ch0)   3 nodes
//    SG1: Gain(N1, ch1) -> RFFT(N1, ch1) -> IFFT(N1, ch1)   3 nodes
//    SG2: Gain(N2, ch2) -> RFFT(N2, ch2) -> IFFT(N2, ch2)   3 nodes
//    SG3: Gain(N3, ch3) -> RFFT(N3, ch3) -> IFFT(N3, ch3)   3 nodes
//
//  Signal data lives in the arena (via BumpAllocator); sinusoidal input
//  is generated at runtime — no Python-generated static reference data.
//
//  5 test categories (35 cases):
//    1. Single subgraph               (IDs 1-10)
//    2. Multi-subgraph, different FFT  (IDs 11-20)
//    3. Multi-subgraph, shared FFT     (IDs 21-28)
//    4. Arena exhaustion               (IDs 29-32)
//    5. Sharing prevents OOM           (IDs 33-35)
// -----------------------------------------------------------------------

#include "TISP_twiddleLUT_signal_chain_memory_map.hpp"
#include "TISP_twiddleLUT_signal_chain_params.h"
#include <TISP_test.h>
#include <TISP_unit_test_l2_heap.hpp>
#include <tisp.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

using TISP::BumpAllocator; // via UnitTestHeap alias in TISP_unit_test_l2_heap.hpp
using TISP::FFTLIB::TwiddleLUT;
#include <TISP_ErrorCtxt.hpp>

/// Twiddle arena budget (must match TISP_bump_allocator twiddle arena).
static constexpr size_t kTwiddleArenaSize = 32u * 1024u;

/// Round-trip tolerance for Gain -> RFFT -> IFFT.
static constexpr float kTolerance = 1e-3f;

/// One scalar gain per subgraph (applied to all channels in the subgraph).
static constexpr float kGains[MAX_SUBGRAPHS] = {0.5f, 0.8f, 1.0f, 0.3f};

/// Pi constant for sinusoidal generation.
static constexpr float kPi = 3.14159265358979323846f;

// -----------------------------------------------------------------------
//  Per-subgraph buffer descriptor (raw pointers into L2SRAM)
// -----------------------------------------------------------------------
struct SubgraphBuffers {
   float   *input;    // N * ch  floats in L2
   float   *gained;   // N * ch  floats in L2
   float   *rfftTemp; // 2N * ch floats in L2
   float   *rfftOut;  // 2N * ch floats in L2
   float   *ifftTemp; // 2N * ch floats in L2
   float   *output;   // N * ch  floats in L2
   float    gainVal;  // scalar (stack)
   uint32_t N;
   uint32_t ch;
};

// -----------------------------------------------------------------------
//  Test state passed between create_graph / execute_graph / delete_graph
// -----------------------------------------------------------------------
struct SignalChainTestState {
   int32_t passCount;
   int32_t failCount;
   FILE   *json;
   FILE   *csv;
   bool    firstResult;
};

// -----------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------

/// Print a check result. Returns the condition for accumulation.
static bool check(bool condition, const char *label)
{
   printf("  %s  %s\n", condition ? "PASS" : "FAIL", label);
   return condition;
}

/// Fill buf[c*N .. c*N+N) with sin(2*pi*freq*(c+1)*i/N) for each channel c.
static void generateSinusoidalInput(float *buf, uint32_t N, uint32_t ch)
{
   for (uint32_t c = 0; c < ch; c++) {
      float freq = 100.0f * static_cast<float>(c + 1);
      for (uint32_t i = 0; i < N; i++) {
         float t        = static_cast<float>(i) / static_cast<float>(N);
         buf[c * N + i] = sinf(2.0f * kPi * freq * t);
      }
   }
}

/// Compute max |output[i] - input[i]*gain| across all channels.
static float computeMaxError(const float *output, const float *input, float gain, uint32_t N, uint32_t ch)
{
   float maxErr = 0.0f;
   for (uint32_t c = 0; c < ch; c++) {
      for (uint32_t i = 0; i < N; i++) {
         float expected = input[c * N + i] * gain;
         float err      = std::fabs(output[c * N + i] - expected);
         if (err > maxErr) {
            maxErr = err;
         }
      }
   }
   return maxErr;
}

/// Sum of 5*Ni*sizeof(float) across unique FFT sizes in this test case.
static size_t computeExpectedLutBytes(const SubgraphTestParams_t &prm)
{
   uint32_t uniqueSizes[MAX_SUBGRAPHS];
   uint32_t numUnique = 0;

   for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
      bool found = false;
      for (uint32_t u = 0; u < numUnique; u++) {
         if (uniqueSizes[u] == prm.fftSize[sg]) {
            found = true;
            break;
         }
      }
      if (!found) {
         uniqueSizes[numUnique++] = prm.fftSize[sg];
      }
   }

   size_t total = 0;
   for (uint32_t u = 0; u < numUnique; u++) {
      total += 5u * uniqueSizes[u] * sizeof(float);
   }
   return total;
}

/// Sum of 5*Ni*sizeof(float) for ALL subgraphs (no deduplication).
static size_t computeNaiveLutBytes(const SubgraphTestParams_t &prm)
{
   size_t total = 0;
   for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
      total += 5u * prm.fftSize[sg] * sizeof(float);
   }
   return total;
}

// -----------------------------------------------------------------------
//  JSON output helpers
// -----------------------------------------------------------------------

static void jsonWriteSubgraphHeader(FILE *f, const SubgraphTestParams_t &prm)
{
   fprintf(f, "      \"testID\": %u,\n", prm.testID);
   fprintf(f, "      \"numSubgraphs\": %u,\n", prm.numSubgraphs);
}

static void
jsonWriteOomResult(FILE *f, const SubgraphTestParams_t &prm, size_t lutTotal, size_t expectedLut, bool allOk)
{
   fprintf(f, "    {\n");
   jsonWriteSubgraphHeader(f, prm);
   fprintf(f, "      \"subgraphs\": [");
   for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
      if (sg > 0)
         fprintf(f, ", ");
      fprintf(f, "{\"fftSize\": %u, \"numChannels\": %u}", prm.fftSize[sg], prm.numChannels[sg]);
   }
   fprintf(f, "],\n");
   fprintf(f, "      \"lutBytes\": %zu,\n", lutTotal);
   fprintf(f, "      \"expectedLutBytes\": %zu,\n", expectedLut);
   fprintf(f, "      \"oom\": true,\n");
   fprintf(f, "      \"result\": \"%s\"\n", allOk ? "PASS" : "FAIL");
   fprintf(f, "    }");
}

// -----------------------------------------------------------------------
//  Entry points (called from main_rtos.c)
// -----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

void *create_graph()
{
   printf("\n");
   printf("=====================================================================\n");
   printf("  TwiddleLUT Signal Chain: Multi-Rate Subgraph Test (%d cases)\n", NUM_TEST_CASES);
   printf("=====================================================================\n\n");

   auto *state        = new SignalChainTestState();
   state->passCount   = 0;
   state->failCount   = 0;
   state->firstResult = true;

   state->json = fopen("TISP_twiddleLUT_signal_chain_results.json", "w");
   if (state->json) {
      fprintf(state->json, "{\n  \"results\": [\n");
   }

   state->csv = fopen("TISP_twiddleLUT_signal_chain_results.csv", "w");
   if (state->csv) {
      fprintf(state->csv, "Test_ID,Num_Subgraphs,FFT_Sizes,Nodes,Max_Error,"
                          "Round_Trip,LUT_KB,Naive_KB,Saved_KB,Expected_LUT_KB,"
                          "Savings_Pct,LUT_Match,Result\n");
   }

   return static_cast<void *>(state);
}

void execute_graph(void *myGraph)
{
   auto &s = *static_cast<SignalChainTestState *>(myGraph);

   for (int32_t tc = 0; tc < NUM_TEST_CASES; tc++) {
      const SubgraphTestParams_t &prm = gTestParams[tc];

      // -- Print test header --
      printf("--------------------------------------------------------------\n");
      printf("  Test %u: %u subgraph(s)", prm.testID, prm.numSubgraphs);
      for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
         printf("  [N=%u ch=%u]", prm.fftSize[sg], prm.numChannels[sg]);
      }
      printf("\n");
      printf("--------------------------------------------------------------\n");

      // -- Fresh arenas for this test case --
      BumpAllocator appHeap(l2Buffer, sizeof(l2Buffer));
      TwiddleLUT<float>::reset();
      TISP::handleAllocator().reset();
      TISP::nodeAllocator().reset();

      // -- Allocate per-subgraph buffers from L2SRAM --
      SubgraphBuffers bufs[MAX_SUBGRAPHS] = {};

      for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
         uint32_t N  = prm.fftSize[sg];
         uint32_t ch = prm.numChannels[sg];

         bufs[sg].N       = N;
         bufs[sg].ch      = ch;
         bufs[sg].gainVal = kGains[sg];

         bufs[sg].input    = appHeap.allocate64ByteAligned<float>(N * ch).release();
         bufs[sg].gained   = appHeap.allocate64ByteAligned<float>(N * ch).release();
         bufs[sg].rfftTemp = appHeap.allocate64ByteAligned<float>(2u * N * ch).release();
         bufs[sg].rfftOut  = appHeap.allocate64ByteAligned<float>(2u * N * ch).release();
         bufs[sg].ifftTemp = appHeap.allocate64ByteAligned<float>(2u * N * ch).release();
         bufs[sg].output   = appHeap.allocate64ByteAligned<float>(N * ch).release();

         // Generate sinusoidal input in L2
         generateSinusoidalInput(bufs[sg].input, N, ch);

         // Zero output buffer
         std::memset(bufs[sg].output, 0, N * ch * sizeof(float));
      }

      // -- Build graph: for each subgraph push Gain -> RFFT -> IFFT --
      auto            myOpVec = new TISP::opVec();
      char            nameBuf[32];
      TISP::ErrorCtxt errorCtx;

      for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
         uint32_t N  = bufs[sg].N;
         uint32_t ch = bufs[sg].ch;

         // Gain: width=N, height=ch, pitch=N*sizeof(float)
         snprintf(nameBuf, sizeof(nameBuf), "Gain_%u", sg);
         auto gain = TISP::make_unique_node<TISP::AUDIOLIB::Gain<float>>(bufs[sg].input, &bufs[sg].gainVal,
                                                                         bufs[sg].gained, N, ch, N * sizeof(float),
                                                                         N * sizeof(float), nameBuf, 3u * sg, errorCtx);
         if (!errorCtx.isSuccess()) {
            printf("  ERROR: Gain_%u construction failed: %s\n", sg, errorCtx.getMessage());
            break;
         }
         errorCtx.reset();

         // RFFT: half spectrum (outMode=false), batched with numChannels=ch
         snprintf(nameBuf, sizeof(nameBuf), "RFFT_%u", sg);
         const TISP::TISP_BufDesc2D rfftIn  = {bufs[sg].gained, TISP::Types::kFloat32, N, ch,
                                            static_cast<size_t>(N) * sizeof(float), 0U};
         const TISP::TISP_BufDesc2D rfftOut = {bufs[sg].rfftOut, TISP::Types::kFloat32, N + 2u, ch,
                                            static_cast<size_t>(N + 2u) * sizeof(float), 0U};

         const TISP::TISP_BufDescList rfftInputs  = {&rfftIn, 1U};
         const TISP::TISP_BufDescList rfftOutputs = {&rfftOut, 1U};

         TISP::FFTLIB::RFFT1dBatchedConfig rfftBuild;

         auto rfft = TISP::make_unique_node<TISP::FFTLIB::RFFT1dBatched<float>>(
             &rfftInputs, &rfftOutputs, bufs[sg].rfftTemp, 2u * N * ch * sizeof(float), &rfftBuild, nameBuf,
             3u * sg + 1u, errorCtx);
         if (!errorCtx.isSuccess()) {
            printf("  ERROR: RFFT_%u construction failed: %s\n", sg, errorCtx.getMessage());
            break;
         }
         errorCtx.reset();

         // IFFT: half spectrum (inMode=false), batched with numChannels=ch
         snprintf(nameBuf, sizeof(nameBuf), "IFFT_%u", sg);
         const TISP::TISP_BufDesc2D ifftIn  = {bufs[sg].rfftOut, TISP::Types::kFloat32, N + 2u, ch,
                                            static_cast<size_t>(N + 2u) * sizeof(float), 0U};
         const TISP::TISP_BufDesc2D ifftOut = {bufs[sg].output, TISP::Types::kFloat32, N, ch,
                                            static_cast<size_t>(N) * sizeof(float), 0U};

         const TISP::TISP_BufDescList ifftInputs  = {&ifftIn, 1U};
         const TISP::TISP_BufDescList ifftOutputs = {&ifftOut, 1U};

         TISP::FFTLIB::IFFTR1dBatchedConfig ifftBuild;

         auto ifft = TISP::make_unique_node<TISP::FFTLIB::IFFTR1dBatched<float>>(
             &ifftInputs, &ifftOutputs, bufs[sg].ifftTemp, 2u * N * ch * sizeof(float), &ifftBuild, nameBuf,
             3u * sg + 2u, errorCtx);
         if (!errorCtx.isSuccess()) {
            printf("  ERROR: IFFT_%u construction failed: %s\n", sg, errorCtx.getMessage());
            break;
         }
         errorCtx.reset();

         myOpVec->push_back(std::move(gain));
         myOpVec->push_back(std::move(rfft));
         myOpVec->push_back(std::move(ifft));
      }

      printf("  Graph: %zu nodes (%u subgraphs x 3)\n", myOpVec->size(), prm.numSubgraphs);

      bool allOk = errorCtx.isSuccess();

      // Twiddle arena exhaustion is reported by the TwiddleLUT through errorCtx during node construction
      bool twiddleOom = !allOk && (std::strstr(errorCtx.getMessage(), "twiddleLUT") != nullptr);

      if (allOk || twiddleOom) {
         // -- LUT accounting --
         size_t expectedLut = computeExpectedLutBytes(prm);
         size_t lutTotal    = TwiddleLUT<float>::totalBytes();
         size_t naiveLut    = computeNaiveLutBytes(prm);
         bool   oomDetected = twiddleOom || (lutTotal < expectedLut);

         char checkBuf[256];

         if (oomDetected) {
            // ============================================================
            //  OOM path: twiddle arena too small — skip execution
            // ============================================================
            bool oomExpected = (expectedLut > kTwiddleArenaSize);

            printf("  OOM detected: LUT has %zu of %zu expected bytes\n", lutTotal, expectedLut);
            printf("  Arena:    %zu bytes (%.1f KB)\n", kTwiddleArenaSize, kTwiddleArenaSize / 1024.0);
            printf("  Required: %zu bytes (%.1f KB)\n", expectedLut, expectedLut / 1024.0);

            snprintf(checkBuf, sizeof(checkBuf), "OOM expected (%.1f KB > %.1f KB arena)", expectedLut / 1024.0,
                     kTwiddleArenaSize / 1024.0);
            allOk = check(oomExpected, checkBuf);

            // JSON — OOM case
            if (s.json) {
               if (!s.firstResult)
                  fprintf(s.json, ",\n");
               s.firstResult = false;
               jsonWriteOomResult(s.json, prm, lutTotal, expectedLut, allOk);
            }

            // CSV — OOM case
            if (s.csv) {
               fprintf(s.csv, "%u,%u,", prm.testID, prm.numSubgraphs);
               for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
                  if (sg > 0)
                     fprintf(s.csv, ";");
                  fprintf(s.csv, "%u", prm.fftSize[sg]);
               }
               fprintf(s.csv, ",%u,,OOM,%.2f,%.2f,%.2f,%.2f,%.1f,%s,%s\n", 3u * prm.numSubgraphs, lutTotal / 1024.0,
                       naiveLut / 1024.0, (naiveLut - lutTotal) / 1024.0, expectedLut / 1024.0,
                       naiveLut > 0 ? (naiveLut - lutTotal) * 100.0 / naiveLut : 0.0,
                       (lutTotal == expectedLut) ? "TRUE" : "FALSE", allOk ? "PASS" : "FAIL");
            }
         }
         else {
            // ============================================================
            //  Normal path: execute graph and verify per-subgraph output
            // ============================================================
            TISP::execute::graph(myOpVec);

            // -- Per-subgraph round-trip verification --
            bool subgraphsOk = true;

            // JSON — open test result
            if (s.json) {
               if (!s.firstResult)
                  fprintf(s.json, ",\n");
               s.firstResult = false;
               fprintf(s.json, "    {\n");
               jsonWriteSubgraphHeader(s.json, prm);
               fprintf(s.json, "      \"subgraphs\": [\n");
            }

            for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
               float maxErr =
                   computeMaxError(bufs[sg].output, bufs[sg].input, bufs[sg].gainVal, bufs[sg].N, bufs[sg].ch);

               snprintf(checkBuf, sizeof(checkBuf), "SG%u round-trip (N=%u ch=%u maxErr=%.2e tol=%.2e)", sg, bufs[sg].N,
                        bufs[sg].ch, maxErr, kTolerance);
               bool ok = check(maxErr < kTolerance, checkBuf);
               subgraphsOk &= ok;

               // JSON — subgraph result
               if (s.json) {
                  if (sg > 0)
                     fprintf(s.json, ",\n");
                  fprintf(s.json,
                          "        {\"fftSize\": %u, \"numChannels\": %u, "
                          "\"maxError\": %.2e, \"pass\": %s}",
                          bufs[sg].N, bufs[sg].ch, maxErr, ok ? "true" : "false");
               }
            }

            if (s.json) {
               fprintf(s.json, "\n      ],\n");
            }

            allOk = subgraphsOk;

            // -- LUT accounting checks --
            printf("  LUT totalBytes: %5zu bytes (%.1f KB)\n", lutTotal, lutTotal / 1024.0);
            printf("  Expected LUT:   %5zu bytes (%.1f KB)\n", expectedLut, expectedLut / 1024.0);
            printf("  Naive LUT:      %5zu bytes (%.1f KB)\n", naiveLut, naiveLut / 1024.0);

            snprintf(checkBuf, sizeof(checkBuf), "LUT matches expected: %zu == %zu", lutTotal, expectedLut);
            allOk &= check(lutTotal == expectedLut, checkBuf);

            // Sharing check (only meaningful when there are duplicate sizes)
            if (naiveLut > expectedLut) {
               snprintf(checkBuf, sizeof(checkBuf), "Sharing saved memory: %zu < %zu bytes", lutTotal, naiveLut);
               allOk &= check(lutTotal < naiveLut, checkBuf);
            }

            // Sharing-prevents-OOM check (category 5)
            bool sharingPreventedOOM = (naiveLut > kTwiddleArenaSize && !oomDetected);
            if (sharingPreventedOOM) {
               snprintf(checkBuf, sizeof(checkBuf), "Sharing prevented OOM: naive %.1f KB > arena %.1f KB",
                        naiveLut / 1024.0, kTwiddleArenaSize / 1024.0);
               check(true, checkBuf); // informational, always PASS
            }

            // JSON — close test result
            if (s.json) {
               fprintf(s.json, "      \"lutBytes\": %zu,\n", lutTotal);
               fprintf(s.json, "      \"expectedLutBytes\": %zu,\n", expectedLut);
               fprintf(s.json, "      \"naiveLutBytes\": %zu,\n", naiveLut);
               fprintf(s.json, "      \"lutMatch\": %s,\n", (lutTotal == expectedLut) ? "true" : "false");
               if (sharingPreventedOOM) {
                  fprintf(s.json, "      \"sharingPreventedOOM\": true,\n");
               }
               fprintf(s.json, "      \"result\": \"%s\"\n", allOk ? "PASS" : "FAIL");
               fprintf(s.json, "    }");
            }

            // CSV — normal case
            if (s.csv) {
               // Compute overall max error across all subgraphs
               float overallMaxErr = 0.0f;
               for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
                  float err =
                      computeMaxError(bufs[sg].output, bufs[sg].input, bufs[sg].gainVal, bufs[sg].N, bufs[sg].ch);
                  if (err > overallMaxErr)
                     overallMaxErr = err;
               }

               fprintf(s.csv, "%u,%u,", prm.testID, prm.numSubgraphs);
               for (uint32_t sg = 0; sg < prm.numSubgraphs; sg++) {
                  if (sg > 0)
                     fprintf(s.csv, ";");
                  fprintf(s.csv, "%u", prm.fftSize[sg]);
               }
               fprintf(s.csv, ",%zu,%.2e,%s,%.2f,%.2f,%.2f,%.2f,%.1f,%s,%s\n", myOpVec->size(), overallMaxErr,
                       subgraphsOk ? "PASS" : "FAIL", lutTotal / 1024.0, naiveLut / 1024.0,
                       (naiveLut - lutTotal) / 1024.0, expectedLut / 1024.0,
                       naiveLut > 0 ? (naiveLut - lutTotal) * 100.0 / naiveLut : 0.0,
                       (lutTotal == expectedLut) ? "TRUE" : "FALSE", allOk ? "PASS" : "FAIL");
            }
         }
      }

      // -- Accumulate pass/fail --
      if (allOk) {
         s.passCount++;
      }
      else {
         s.failCount++;
      }

      // -- Cleanup --
      delete myOpVec;
      TwiddleLUT<float>::reset();
      // appHeap goes out of scope — L2 arena reclaimed on next iteration
   }
}

void check_output()
{
   // All checks are inline in execute_graph.
}

void delete_graph(void *myGraph)
{
   auto *s = static_cast<SignalChainTestState *>(myGraph);

   printf("\n=====================================================================\n");
   printf("  Results: %d PASSED, %d FAILED (of %d)\n", s->passCount, s->failCount, NUM_TEST_CASES);
   printf("=====================================================================\n");

   if (s->failCount == 0) {
      printf("  ALL TESTS PASSED\n");
   }
   else {
      printf("  *** FAILURES DETECTED ***\n");
   }

   if (s->json) {
      fprintf(s->json, "\n  ],\n");
      fprintf(s->json, "  \"summary\": {\"total\": %d, \"passed\": %d, \"failed\": %d}\n", NUM_TEST_CASES, s->passCount,
              s->failCount);
      fprintf(s->json, "}\n");
      fclose(s->json);
      printf("  JSON report: TISP_twiddleLUT_signal_chain_results.json\n");
   }

   if (s->csv) {
      fclose(s->csv);
      printf("  CSV  report: TISP_twiddleLUT_signal_chain_results.csv\n");
   }

   delete s;
}

#ifdef __cplusplus
}
#endif
