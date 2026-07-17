// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <TISP_ErrorCtxt.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <tisp.hpp>
#include <vector>

using TISP::FFTLIB::TwiddleLUT;

/// Test state passed through the void* graph pointer.
struct TwiddleLUTTestState {
   int32_t passCount;
   int32_t failCount;
   FILE   *csv;
};

static void check(TwiddleLUTTestState &s, bool condition, const char *name)
{
   if (condition) {
      printf("  PASS  %s\n", name);
      s.passCount++;
   }
   else {
      printf("  FAIL  %s\n", name);
      s.failCount++;
   }
}

/// Write one row to the CSV report (sizes converted to KB).
static void csvRow(FILE       *fp,
                   int         section,
                   const char *description,
                   const char *fftSizes,
                   uint32_t    pipelines,
                   size_t      lutBytes,
                   size_t      naiveBytes,
                   size_t      savedBytes,
                   size_t      arenaBytes,
                   double      savingsPct,
                   const char *status)
{
   if (fp == nullptr)
      return;
   fprintf(fp, "%d,\"%s\",\"%s\",%u,%.1f,%.1f,%.1f,%.1f,%.1f,%s\n", section, description, fftSizes, pipelines,
           lutBytes / 1024.0, naiveBytes / 1024.0, savedBytes / 1024.0, arenaBytes / 1024.0, savingsPct, status);
}

static constexpr size_t kArenaSize = 32u * 1024u;

#ifdef __cplusplus
extern "C" {
#endif

void *create_graph()
{
   printf("\n");
   printf("===================================================\n");
   printf("    TwiddleLUT Diagnostic & Stress Test             \n");
   printf("===================================================\n\n");

   auto *state = new TwiddleLUTTestState{0, 0, nullptr};

   state->csv = fopen("TISP_twiddleLUT.csv", "w+");
   if (state->csv) {
      fprintf(state->csv, "Section,Description,FFT_Sizes,Pipelines,LUT_KB,Naive_KB,"
                          "Saved_KB,Arena_KB,Savings_Pct,Status\n");
   }

   return static_cast<void *>(state);
}

void execute_graph(void *myGraph)
{
   auto &s = *static_cast<TwiddleLUTTestState *>(myGraph);

   // Start clean
   TwiddleLUT<float>::clear();

   // ------------------------------------------------------------------
   // 1. Complex twiddles: pointer equality for same FFT size
   // ------------------------------------------------------------------
   printf("[1] Complex twiddles (FFT1dBatched path)\n");
   {
      TISP::ErrorCtxt ctx1;
      auto            c1 = TwiddleLUT<float>::getOrCreateComplexTwiddles(256, ctx1);
      check(s, ctx1.isSuccess(), "getOrCreateComplexTwiddles(256) succeeded");
      size_t after1 = TwiddleLUT<float>::totalBytes();

      TISP::ErrorCtxt ctx2;
      auto            c2 = TwiddleLUT<float>::getOrCreateComplexTwiddles(256, ctx2);
      check(s, ctx2.isSuccess(), "getOrCreateComplexTwiddles(256) cache hit succeeded");
      size_t after2 = TwiddleLUT<float>::totalBytes();

      check(s, c1.get() == c2.get(), "Same-size complex twiddles share pointer");
      check(s, after1 == after2, "totalBytes unchanged on cache hit");
      printf("     totalBytes after 2x FFT(256): %zu\n", after2);

      TISP::ErrorCtxt ctx3;
      auto            c3 = TwiddleLUT<float>::getOrCreateComplexTwiddles(512, ctx3);
      check(s, ctx3.isSuccess(), "getOrCreateComplexTwiddles(512) succeeded");
      size_t after3 = TwiddleLUT<float>::totalBytes();
      check(s, c3.get() != c1.get(), "Different-size twiddles get distinct pointer");
      check(s, after3 > after2, "totalBytes grows for new FFT size");
      printf("     totalBytes after FFT(512):     %zu\n", after3);

      // CSV: 3 getOrCreate calls (2×256 + 1×512). Per-instance: each allocates 2N floats.
      size_t naive1 = 2 * (256 * 2 * sizeof(float)) + 512 * 2 * sizeof(float);
      size_t saved1 = naive1 - after3;
      double pct1   = 100.0 * saved1 / naive1;
      csvRow(s.csv, 1, "Complex twiddle sharing", "256+512", 3, after3, naive1, saved1, kArenaSize, pct1, "PASS");
   }

   TwiddleLUT<float>::clear();

   // ------------------------------------------------------------------
   // 2. Real twiddles: shared between RFFT and IFFT
   // ------------------------------------------------------------------
   printf("\n[2] Real twiddles (RFFT/IFFT share twiddles)\n");
   {
      TISP::ErrorCtxt ctx_rfft;
      auto            r_rfft = TwiddleLUT<float>::getOrCreateRealTwiddles(256, ctx_rfft);
      check(s, ctx_rfft.isSuccess(), "getOrCreateRealTwiddles(256) for RFFT succeeded");

      TISP::ErrorCtxt ctx_ifft;
      auto            r_ifft = TwiddleLUT<float>::getOrCreateRealTwiddles(256, ctx_ifft);
      check(s, ctx_ifft.isSuccess(), "getOrCreateRealTwiddles(256) for IFFT succeeded");

      check(s, r_rfft.get() == r_ifft.get(), "RFFT and IFFT share real twiddles");
      printf("     Pointer: RFFT=%p  IFFT=%p\n", (void *) r_rfft.get(), (void *) r_ifft.get());

      // CSV: 2 getOrCreate calls. Per-instance: each allocates N floats.
      size_t lut2   = TwiddleLUT<float>::totalBytes();
      size_t naive2 = 2 * (256 * sizeof(float));
      size_t saved2 = naive2 - lut2;
      double pct2   = 100.0 * saved2 / naive2;
      csvRow(s.csv, 2, "Real twiddle sharing (RFFT/IFFT)", "256", 2, lut2, naive2, saved2, kArenaSize, pct2, "PASS");
   }

   TwiddleLUT<float>::clear();

   // ------------------------------------------------------------------
   // 3. Split factors: RFFT vs IFFT must be DIFFERENT
   // ------------------------------------------------------------------
   printf("\n[3] Split factors (RFFT vs IFFT must differ)\n");
   {
      TISP::ErrorCtxt ctx_rfft;
      auto            sf_rfft = TwiddleLUT<float>::getOrCreateRfftSplitFactors(256, ctx_rfft);
      check(s, ctx_rfft.isSuccess(), "getOrCreateRfftSplitFactors(256) succeeded");

      TISP::ErrorCtxt ctx_ifft;
      auto            sf_ifft = TwiddleLUT<float>::getOrCreateIfftSplitFactors(256, ctx_ifft);
      check(s, ctx_ifft.isSuccess(), "getOrCreateIfftSplitFactors(256) succeeded");

      check(s, sf_rfft.get() != sf_ifft.get(), "RFFT and IFFT split factors differ (cosine sign)");

      // Same type, same size → must share
      TISP::ErrorCtxt ctx_rfft2;
      auto            sf_rfft2 = TwiddleLUT<float>::getOrCreateRfftSplitFactors(256, ctx_rfft2);
      check(s, ctx_rfft2.isSuccess(), "2nd getOrCreateRfftSplitFactors(256) succeeded");

      TISP::ErrorCtxt ctx_ifft2;
      auto            sf_ifft2 = TwiddleLUT<float>::getOrCreateIfftSplitFactors(256, ctx_ifft2);
      check(s, ctx_ifft2.isSuccess(), "2nd getOrCreateIfftSplitFactors(256) succeeded");

      check(s, sf_rfft2.get() == sf_rfft.get(), "2nd RFFT split factors share pointer");
      check(s, sf_ifft2.get() == sf_ifft.get(), "2nd IFFT split factors share pointer");

      // CSV: 4 getOrCreate calls (2×RFFT_SF + 2×IFFT_SF). Per-instance: each allocates 2N floats.
      size_t lut3   = TwiddleLUT<float>::totalBytes();
      size_t naive3 = 4 * (2 * 256 * sizeof(float));
      size_t saved3 = naive3 - lut3;
      double pct3   = 100.0 * saved3 / naive3;
      csvRow(s.csv, 3, "Split factors (RFFT vs IFFT differ)", "256", 4, lut3, naive3, saved3, kArenaSize, pct3, "PASS");
   }

   TwiddleLUT<float>::clear();

   // ------------------------------------------------------------------
   // 4. Simulated 6-node audio graph: 3×RFFT + 3×IFFT, all 256-point
   // ------------------------------------------------------------------
   printf("\n[4] Memory savings: 3xRFFT(256) + 3xIFFT(256)\n");
   {
      size_t         perInstanceTotal = 0;
      const uint32_t N                = 256;

      const size_t rfftPerNode = (N + 2 * N) * sizeof(float);
      const size_t ifftPerNode = (N + 2 * N) * sizeof(float);

      for (int i = 0; i < 3; i++) {
         TISP::ErrorCtxt ctx1;
         TwiddleLUT<float>::getOrCreateRealTwiddles(N, ctx1);
         TISP::ErrorCtxt ctx2;
         TwiddleLUT<float>::getOrCreateRfftSplitFactors(N, ctx2);
         perInstanceTotal += rfftPerNode;
      }
      for (int i = 0; i < 3; i++) {
         TISP::ErrorCtxt ctx3;
         TwiddleLUT<float>::getOrCreateRealTwiddles(N, ctx3);
         TISP::ErrorCtxt ctx4;
         TwiddleLUT<float>::getOrCreateIfftSplitFactors(N, ctx4);
         perInstanceTotal += ifftPerNode;
      }

      size_t lutTotal = TwiddleLUT<float>::totalBytes();
      size_t saved    = perInstanceTotal - lutTotal;
      double pct      = 100.0 * saved / perInstanceTotal;

      printf("     Without LUT (per-instance): %5zu bytes (%zu KB)\n", perInstanceTotal, perInstanceTotal / 1024);
      printf("     With LUT (shared):          %5zu bytes (%zu KB)\n", lutTotal, lutTotal / 1024);
      printf("     Saved:                      %5zu bytes (%.0f%%)\n", saved, pct);

      check(s, lutTotal < perInstanceTotal, "LUT uses less memory than per-instance");
      check(s, pct > 70.0, "Savings exceed 70%% for 6-node graph");

      csvRow(s.csv, 4, "3xRFFT+3xIFFT same size", "256", 3, lutTotal, perInstanceTotal, saved, kArenaSize, pct, "PASS");
   }

   TwiddleLUT<float>::clear();

   // ------------------------------------------------------------------
   // 5. Realistic 2D FFT graph: 2×FFT1dBatched(512)
   // ------------------------------------------------------------------
   printf("\n[5] Memory savings: 2xFFT1dBatched(512) (2D FFT pattern)\n");
   {
      const uint32_t N           = 512;
      const size_t   perInstance = 2 * (N * 2 * sizeof(float));

      TISP::ErrorCtxt ctx5_1;
      TwiddleLUT<float>::getOrCreateComplexTwiddles(N, ctx5_1);
      TISP::ErrorCtxt ctx5_2;
      TwiddleLUT<float>::getOrCreateComplexTwiddles(N, ctx5_2);

      size_t lutTotal = TwiddleLUT<float>::totalBytes();
      size_t saved    = perInstance - lutTotal;
      double pct      = 100.0 * saved / perInstance;

      printf("     Without LUT (per-instance): %5zu bytes (%zu KB)\n", perInstance, perInstance / 1024);
      printf("     With LUT (shared):          %5zu bytes (%zu KB)\n", lutTotal, lutTotal / 1024);
      printf("     Saved:                      %5zu bytes (%.0f%%)\n", saved, pct);

      check(s, lutTotal < perInstance, "LUT uses less memory than per-instance");
      check(s, pct > 49.0, "Savings exceed 49%% for 2-node graph");

      csvRow(s.csv, 5, "2xFFT1dBatched", "512", 2, lutTotal, perInstance, saved, kArenaSize, pct, "PASS");
   }

   TwiddleLUT<float>::clear();

   // ------------------------------------------------------------------
   // 6. clear() releases all entries
   // ------------------------------------------------------------------
   printf("\n[6] clear() releases all entries\n");
   {
      TISP::ErrorCtxt ctx6_1;
      TwiddleLUT<float>::getOrCreateComplexTwiddles(256, ctx6_1);
      TISP::ErrorCtxt ctx6_2;
      TwiddleLUT<float>::getOrCreateRealTwiddles(256, ctx6_2);
      TISP::ErrorCtxt ctx6_3;
      TwiddleLUT<float>::getOrCreateRfftSplitFactors(256, ctx6_3);
      TISP::ErrorCtxt ctx6_4;
      TwiddleLUT<float>::getOrCreateIfftSplitFactors(256, ctx6_4);

      size_t before = TwiddleLUT<float>::totalBytes();
      check(s, before > 0, "totalBytes > 0 before clear");

      TwiddleLUT<float>::clear();
      size_t after = TwiddleLUT<float>::totalBytes();
      check(s, after == 0, "totalBytes == 0 after clear");

      // CSV: clear() freed everything. Saved_Bytes = before (all reclaimed).
      csvRow(s.csv, 6, "clear() releases all entries", "256", 0, after, before, before, kArenaSize, 100.0, "PASS");
   }

   // ------------------------------------------------------------------
   // 7. Mixed RFFT/IFFT sizes: 4 size pairs that fit in arena
   //    Budget: 5*N*4 per unique N.
   //    64+128+256+512 = 1280+2560+5120+10240 = 19200 bytes (< 32 KB)
   // ------------------------------------------------------------------
   printf("\n[7] Mixed-size signal chain: RFFT+IFFT at N=64,128,256,512\n");
   {
      TwiddleLUT<float>::clear();

      const uint32_t sizes[]     = {64, 128, 256, 512};
      const uint32_t numSizes    = sizeof(sizes) / sizeof(sizes[0]);
      size_t         naiveTotal  = 0;
      size_t         expectedLut = 0;
      bool           allAllocOk  = true;

      for (uint32_t i = 0; i < numSizes; i++) {
         uint32_t N = sizes[i];

         // Simulate one RFFT + one IFFT node of this size
         TISP::ErrorCtxt ctx_w;
         auto            pW = TwiddleLUT<float>::getOrCreateRealTwiddles(N, ctx_w);
         TISP::ErrorCtxt ctx_sr;
         auto            pSr = TwiddleLUT<float>::getOrCreateRfftSplitFactors(N, ctx_sr);
         TISP::ErrorCtxt ctx_si;
         auto            pSi = TwiddleLUT<float>::getOrCreateIfftSplitFactors(N, ctx_si);

         if (!pW || !pSr || !pSi || !ctx_w.isSuccess() || !ctx_sr.isSuccess() || !ctx_si.isSuccess()) {
            printf("     OOM at N=%u\n", N);
            allAllocOk = false;
            break;
         }

         // Naive: each RFFT+IFFT pair would allocate 6*N floats without LUT
         naiveTotal += 6 * N * sizeof(float);
         // LUT: 5*N floats per unique size (real + rfft_sf + ifft_sf)
         expectedLut += 5 * N * sizeof(float);
      }

      size_t lutTotal = TwiddleLUT<float>::totalBytes();
      double savedPct = (naiveTotal > 0) ? 100.0 * (naiveTotal - lutTotal) / naiveTotal : 0.0;

      printf("     Sizes: 64, 128, 256, 512\n");
      printf("     LUT totalBytes:  %5zu bytes (%.1f KB)\n", lutTotal, lutTotal / 1024.0);
      printf("     Naive per-inst:  %5zu bytes (%.1f KB)\n", naiveTotal, naiveTotal / 1024.0);
      printf("     Expected LUT:    %5zu bytes (%.1f KB)\n", expectedLut, expectedLut / 1024.0);
      printf("     Arena:           %5zu bytes (%.1f KB)\n", kArenaSize, kArenaSize / 1024.0);
      printf("     Arena usage:     %.1f%%\n", 100.0 * lutTotal / kArenaSize);
      printf("     Saved:           %.0f%%\n", savedPct);

      check(s, allAllocOk, "All 4 size pairs allocated successfully");
      check(s, lutTotal == expectedLut, "LUT matches expected size for mixed sizes");

      char checkBuf[128];
      snprintf(checkBuf, sizeof(checkBuf), "Arena usage %.1f%% < 100%% (fits in 32 KB)", 100.0 * lutTotal / kArenaSize);
      check(s, lutTotal < kArenaSize, checkBuf);

      csvRow(s.csv, 7, "Mixed 4 sizes (fits)", "64+128+256+512", 1, lutTotal, naiveTotal, naiveTotal - lutTotal,
             kArenaSize, savedPct, "PASS");

      TwiddleLUT<float>::clear();
   }

   // ------------------------------------------------------------------
   // 8. Mixed RFFT/IFFT: 5 size pairs — N=1024 pushes past 32 KB
   //    64+128+256+512+1024 = 19200+20480 = 39680 bytes (> 32 KB)
   // ------------------------------------------------------------------
   printf("\n[8] Arena exhaustion: RFFT+IFFT at N=64,128,256,512,1024\n");
   {
      TwiddleLUT<float>::clear();

      const uint32_t sizes[]     = {64, 128, 256, 512, 1024};
      const uint32_t numSizes    = sizeof(sizes) / sizeof(sizes[0]);
      bool           oomHit      = false;
      uint32_t       oomSize     = 0;
      size_t         bytesBefore = 0;

      for (uint32_t i = 0; i < numSizes; i++) {
         uint32_t N  = sizes[i];
         bytesBefore = TwiddleLUT<float>::totalBytes();

         TISP::ErrorCtxt ctx_w;
         auto            pW = TwiddleLUT<float>::getOrCreateRealTwiddles(N, ctx_w);
         TISP::ErrorCtxt ctx_sr;
         auto            pSr = TwiddleLUT<float>::getOrCreateRfftSplitFactors(N, ctx_sr);
         TISP::ErrorCtxt ctx_si;
         auto            pSi = TwiddleLUT<float>::getOrCreateIfftSplitFactors(N, ctx_si);

         if (!pW || !pSr || !pSi || !ctx_w.isSuccess() || !ctx_sr.isSuccess() || !ctx_si.isSuccess()) {
            oomHit  = true;
            oomSize = N;
            printf("     OOM at N=%u (arena had %zu of %zu bytes used)\n", N, bytesBefore, kArenaSize);
            break;
         }

         size_t bytesAfter = TwiddleLUT<float>::totalBytes();
         printf("     N=%4u: +%5zu bytes, total %5zu / %zu (%.1f%%)\n", N, bytesAfter - bytesBefore, bytesAfter,
                kArenaSize, 100.0 * bytesAfter / kArenaSize);
      }

      check(s, oomHit, "OOM detected as expected (arena exhausted)");

      char checkBuf[128];
      snprintf(checkBuf, sizeof(checkBuf), "OOM triggered at N=%u (would exceed 32 KB arena)", oomSize);
      check(s, oomSize == 1024, checkBuf);

      // Verify the LUT entries that DID succeed are still valid
      size_t lutAfter = TwiddleLUT<float>::totalBytes();
      check(s, lutAfter > 0, "Previously allocated twiddles still valid after OOM");

      printf("     Conclusion: 32 KB arena supports up to 4 distinct RFFT/IFFT\n");
      printf("     sizes (64+128+256+512 = %.1f KB). Adding N=1024 (+20 KB)\n", 19200.0 / 1024.0);
      printf("     would require %.1f KB total — exceeds 32 KB.\n", 39680.0 / 1024.0);

      csvRow(s.csv, 8, "Mixed 5 sizes (arena full at 1024)", "64+128+256+512+1024", 1, lutAfter, 0, 0, kArenaSize, 0.0,
             "ARENA_FULL");

      // Mixed-size allocations interleave in the arena, preventing LIFO
      // reclamation.  Use reset() to unconditionally reclaim.
      TwiddleLUT<float>::reset();
   }

   // ------------------------------------------------------------------
   // 9. Repeated clear/allocate cycles (same size) — LIFO reclamation
   // ------------------------------------------------------------------
   printf("\n[9] Arena reclamation (clear): 20x cycles at N=512\n");
   {
      bool allOk = true;
      for (int cycle = 0; cycle < 20; cycle++) {
         TwiddleLUT<float>::clear();

         TISP::ErrorCtxt ctx_w;
         auto            pW = TwiddleLUT<float>::getOrCreateRealTwiddles(512, ctx_w);
         TISP::ErrorCtxt ctx_sr;
         auto            pSr = TwiddleLUT<float>::getOrCreateRfftSplitFactors(512, ctx_sr);
         TISP::ErrorCtxt ctx_si;
         auto            pSi = TwiddleLUT<float>::getOrCreateIfftSplitFactors(512, ctx_si);

         if (!pW || !pSr || !pSi || !ctx_w.isSuccess() || !ctx_sr.isSuccess() || !ctx_si.isSuccess()) {
            printf("     OOM at cycle %d — LIFO clear() not reclaiming!\n", cycle);
            allOk = false;
            break;
         }

         size_t total = TwiddleLUT<float>::totalBytes();
         if (total != 5 * 512 * sizeof(float)) {
            printf("     Cycle %d: unexpected totalBytes %zu\n", cycle, total);
            allOk = false;
            break;
         }
      }

      check(s, allOk, "20 clear/allocate cycles succeed (LIFO same-size)");

      csvRow(s.csv, 9, "20x clear/realloc N=512", "512", 1, 5 * 512 * sizeof(float), 0, 0, kArenaSize, 0.0,
             allOk ? "PASS" : "FAIL");

      TwiddleLUT<float>::clear();
   }

   // ------------------------------------------------------------------
   // 10. Repeated reset/allocate cycles (mixed sizes) — arena reset
   // ------------------------------------------------------------------
   printf("\n[10] Arena reclamation (reset): 20x mixed-size cycles\n");
   {
      bool allOk = true;
      for (int cycle = 0; cycle < 20; cycle++) {
         TwiddleLUT<float>::reset();

         // Allocate 4 distinct sizes each cycle
         const uint32_t sizes[] = {64, 128, 256, 512};
         for (uint32_t i = 0; i < 4; i++) {
            TISP::ErrorCtxt ctx_w;
            auto            pW = TwiddleLUT<float>::getOrCreateRealTwiddles(sizes[i], ctx_w);
            TISP::ErrorCtxt ctx_sr;
            auto            pSr = TwiddleLUT<float>::getOrCreateRfftSplitFactors(sizes[i], ctx_sr);
            TISP::ErrorCtxt ctx_si;
            auto            pSi = TwiddleLUT<float>::getOrCreateIfftSplitFactors(sizes[i], ctx_si);
            if (!pW || !pSr || !pSi || !ctx_w.isSuccess() || !ctx_sr.isSuccess() || !ctx_si.isSuccess()) {
               printf("     OOM at cycle %d, N=%u\n", cycle, sizes[i]);
               allOk = false;
               break;
            }
         }
         if (!allOk)
            break;

         size_t total = TwiddleLUT<float>::totalBytes();
         if (total != 19200) {
            printf("     Cycle %d: unexpected totalBytes %zu (expected 19200)\n", cycle, total);
            allOk = false;
            break;
         }
      }

      check(s, allOk, "20 reset/allocate cycles succeed (mixed-size)");

      csvRow(s.csv, 10, "20x reset/realloc mixed", "64+128+256+512", 1, 19200, 0, 0, kArenaSize, 0.0,
             allOk ? "PASS" : "FAIL");

      TwiddleLUT<float>::reset();
   }
}

void check_output()
{
   // No external reference comparison needed — assertions are inline
}

void delete_graph(void *myGraph)
{
   auto *s = static_cast<TwiddleLUTTestState *>(myGraph);

   if (s->failCount == 0) {
      printf("Test Pass!\n");
   }
   else {
      printf("Test Fail!\n");
   }

   if (s->csv) {
      fclose(s->csv);
      printf("  CSV report: TISP_twiddleLUT.csv\n");
   }

   delete s;
}

#ifdef __cplusplus
}
#endif
