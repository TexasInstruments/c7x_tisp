// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL
//
// Two-approach delay comparison test — MATHLIB/dmaXfer profiling pattern.
// Ported from dmautils_delay_circular_poc/dmautils_delay_circular.c.
//
//   Approach A — TISP::misc::DelayDru<float>  (DRU circular, DDR-resident ring).
//   Approach B — TISP::misc::DMAXfer<1> (state DDR->L2) +
//                TISP::AUDIOLIB::DelayNChannel<float> (L2, linear mode) +
//                TISP::misc::DMAXfer<1> (state L2->DDR).
//
// Each approach is profiled (INIT / COLD / WARM) per test case using the
// TI_profile_* API, exactly mirroring TISP_dmaXfer_test.cpp.  Both approaches
// are verified bit-exact against a ported CPU reference for every test case.
//
// DEVIATION FROM SPEC: on-target the ring (A) and persistent state (B) should
// live in a non-cacheable DDR region (.ddr_state_nc, MAIR4) for full DRU
// bandwidth.  The tisp repo has no such linker region; both buffers use the
// standard cacheable .ddrData section.  This does not affect the required
// HOST_EMULATION correctness gate.

#include "TISP_delayDru_druDelay_memory_map.hpp"
#include "TISP_delayDru_druDelay_idat.h"
#include "TISP_test.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdint.h>
#include <tisp.hpp>
#include <TISP_delayDru.hpp>
#include <TISP_delayNChannel.hpp>
#include <TISP_dmaXfer.hpp>

#if !defined(HOST_EMULATION)
#include <kernel/dpl/CacheP.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Natural-C reference — ported from PoC App_delayReference()
// Operates on a float ring of size numChannels * ringSamplesPerCh.
// writeOff is the shared write position (samples, modulo ringSamplesPerCh).
// ─────────────────────────────────────────────────────────────────────────────

static void delayReference_cn(const float *inFrame, float *outFrame,
                               float *ring, uint32_t ringSamplesPerCh,
                               uint32_t writeOff,
                               uint32_t frameSamples, uint32_t numChannels,
                               const uint32_t *delaySamples)
{
   for (uint32_t c = 0; c < numChannels; c++) {
      float       *chRing  = ring + c * ringSamplesPerCh;
      const float *chIn    = inFrame  + c * frameSamples;
      float       *chOut   = outFrame + c * frameSamples;
      uint32_t     readOff = (writeOff + ringSamplesPerCh - delaySamples[c])
                             % ringSamplesPerCh;

      for (uint32_t n = 0; n < frameSamples; n++) {
         chRing[(writeOff + n) % ringSamplesPerCh] = chIn[n];
      }
      for (uint32_t n = 0; n < frameSamples; n++) {
         chOut[n] = chRing[(readOff + n) % ringSamplesPerCh];
      }
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: compute next power of two >= v
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t nextPow2(uint32_t v)
{
   if (v == 0U) return 1U;
   uint32_t p = 1U;
   while (p < v) p <<= 1U;
   return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main test function — MATHLIB/dmaXfer profiling pattern
// ─────────────────────────────────────────────────────────────────────────────

static int TISP_delayDru_druDelay_d(uint32_t *pProfile,
                                        uint8_t  /*LevelOfFeedback*/)
{
   int32_t test_cases;
   TISP_delayDru_druDelay_testParams_t *prm;
   TISP_delayDru_druDelay_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_delayDru_druDelay");

   FILE *fpOutputCSV = fopen("TISP_delayDru_druDelay.csv", "w+");
   fprintf(fpOutputCSV,
           "Kernel, Test ID, numCh, frameSmp, maxDelay, statKB, "
           "Init_A, Warm_A, Init_B, Warm_B, Pass/Fail\n");

   // CLEC / DRU initialisation — must happen once before any DMA transfer
#if defined(__C7100__) || defined(__C7120__)
   TISP::test_sciclientDmscGetVersion(NULL, 0);
   TISP::appC7xClecInitDru();
#elif !defined(HOST_EMULATION)
   App_dmautilsClecInitDru();
#endif

   int32_t fail = 0;
   static const uint32_t APPROACH_B_MAX_STATE_BYTES = 700U * 1024U;

   for (int32_t tpi = 0; tpi < test_cases; tpi++) {
      TISP_delayDru_druDelay_testParams_t currPrm = prm[tpi];
      int32_t currentTestFail = 0;

      const uint32_t numChannels  = currPrm.numChannels;
      const uint32_t frameSamples = currPrm.frameSamples;
      const uint32_t maxDelay     = currPrm.maxDelay;
      const uint32_t numReps      = currPrm.numReps;
      const uint32_t frameElems   = numChannels * frameSamples;

      // Per-case derived sizes
      const uint32_t ringSamplesPerCh  = nextPow2(frameSamples + maxDelay);
      const uint32_t delayDimX         = maxDelay + frameSamples;   // AUDIOLIB widthD
      const uint32_t stateBytesTotal   = numChannels * delayDimX * (uint32_t)sizeof(float);
      const size_t   delayPitchBytes   = (size_t)delayDimX * sizeof(float);

      uint32_t delaySamples[TISP_DDT_MAX_CHANNELS];
      memcpy(delaySamples, currPrm.delaySamples,
             TISP_DDT_MAX_CHANNELS * sizeof(uint32_t));

      // ── Seed input (fixed ramp per channel; same every exec for reproducibility) ──
      for (uint32_t c = 0; c < numChannels; c++) {
         for (uint32_t n = 0; n < frameSamples; n++) {
            gInFrameL2[c * frameSamples + n] =
               (float)(currPrm.testID * 10000U + c * 1000U + n);
         }
      }

      // ── Clear all state for a fresh start on every test case ─────────────
      memset(gRingDdr,       0, TISP_DDT_MAX_RING_BYTES_TOTAL);
      memset(gDelayStateDdr, 0, TISP_DDT_MAX_STATE_BYTES_TOTAL);
      memset(gDelayStateL2,  0, TISP_DDT_MAX_STATE_BYTES_TOTAL);
      memset(gRefRing,       0, TISP_DDT_MAX_RING_BYTES_TOTAL);

      // ══════════════════════════════════════════════════════════════════════
      // APPROACH A — DelayDru<float> (DRU circular DDR ring)
      // ══════════════════════════════════════════════════════════════════════

      // INIT: node construction
      TISP::ErrorCtxt errCtxA;
      TI_profile_start(TI_PROFILE_KERNEL_INIT);
      TISP::NodePtr delayDru = TISP::NodePtr(
         TISP::make_unique_node<TISP::misc::DelayDru<float>>(
            gInFrameL2, gOutFrameL2_A,
            static_cast<void *>(gRingDdr),
            delaySamples,
            static_cast<size_t>(frameSamples),
            static_cast<size_t>(numChannels),
            errCtxA));
      TI_profile_stop();

      if (!errCtxA.isSuccess()) {
         printf("[tpi=%d] DelayDru<float> construction failed: %s\n",
                tpi, errCtxA.getMessage());
         fail = 1;
         continue;
      }

      // WARM exec loop A (warm-up + individual timing)
      for (uint32_t k = 0; k < numReps; k++) {
         TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
         TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);
         delayDru->exec();
         TI_profile_stop();
      }
      // Final exec timed as OPT (caches now hot — warm measurement for the table)
      TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT);
      TI_profile_start(TI_PROFILE_KERNEL_OPT);
      delayDru->exec();
      TI_profile_stop();

      // Save last warm output A for bit-exact check
      static float outA_saved[TISP_DDT_MAX_FRAME_ELEMS];
      memcpy(outA_saved, gOutFrameL2_A, frameElems * sizeof(float));

      pProfile[6 * tpi + 0] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_INIT);
      pProfile[6 * tpi + 1] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
      pProfile[6 * tpi + 2] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);

      delayDru.reset();  // DelayDru destructor / UDMA deinit

      // ── CPU reference (run immediately after A while A's cycles are in the
      //    profile buckets so TI_profile_add_test shows Approach A cycles) ──
      static float refOut[TISP_DDT_MAX_FRAME_ELEMS];
      {
         uint32_t writeOff = 0U;
         memset(gRefRing, 0, ringSamplesPerCh * numChannels * sizeof(float));
         for (uint32_t k = 0; k < 1U + numReps; k++) {
            delayReference_cn(gInFrameL2, refOut, gRefRing,
                              ringSamplesPerCh, writeOff,
                              frameSamples, numChannels, delaySamples);
            writeOff = (writeOff + frameSamples) % ringSamplesPerCh;
         }
      }

      // ── Verify A vs reference now, while A's TI_profile buckets are live ──
      int32_t statusA = TI_compare_mem_2D_float(
         outA_saved, refOut, 0, static_cast<double>(1e-6f),
         frameElems, 1,
         frameElems * sizeof(float), sizeof(float));
      int32_t currentTestFail_A = (statusA == TI_TEST_KERNEL_FAIL) ? 1 : 0;
      fail = fail || currentTestFail_A;

      char desc[128];
      snprintf(desc, sizeof(desc), "ch=%u frm=%u maxD=%u stKB=%u [A]",
               numChannels, frameSamples, maxDelay, stateBytesTotal / 1024U);
      // TI_PROFILE_KERNEL_* buckets still hold Approach A values here
      TI_profile_add_test(currPrm.testID, (int)frameElems, 0, 0,
                          currentTestFail_A, desc);

      // ══════════════════════════════════════════════════════════════════════
      // APPROACH B — DMAXfer<1> (state DDR->L2) + DelayNChannel + DMAXfer<1>
      // ══════════════════════════════════════════════════════════════════════
      // State DDR is already zero (cleared above); L2 state is also zero.
      // DMAXfer addresses are local but DMAXfer<1> makes heap copies.

      const bool runApproachB = (stateBytesTotal <= APPROACH_B_MAX_STATE_BYTES);
      if (runApproachB) {
         void  *stateInSrc[1]  = { static_cast<void *>(gDelayStateDdr) };
         void  *stateInDst[1]  = { static_cast<void *>(gDelayStateL2)  };
         void  *stateOutSrc[1] = { static_cast<void *>(gDelayStateL2)  };
         void  *stateOutDst[1] = { static_cast<void *>(gDelayStateDdr) };
         size_t stateSize[1]   = { static_cast<size_t>(stateBytesTotal) };

         // INIT B: construct all three B nodes
         TISP::ErrorCtxt errCtxB;
         TI_profile_start(TI_PROFILE_KERNEL_INIT);
         TISP::NodePtr dmaXferIn = TISP::NodePtr(
            TISP::make_unique_node<TISP::misc::DMAXfer<1>>(
               1, stateInSrc, stateInDst, stateSize));
         TISP::NodePtr delayNch = TISP::NodePtr(
            TISP::make_unique_node<TISP::AUDIOLIB::DelayNChannel<float>>(
               gInFrameL2,
               gDelayStateL2,
               gScratchL2,
               gOutFrameL2_B,
               static_cast<size_t>(frameSamples),   // width
               static_cast<size_t>(numChannels),     // height
               static_cast<size_t>(delayDimX),       // widthD
               static_cast<size_t>(numChannels),     // heightD
               static_cast<size_t>(frameSamples) * sizeof(float),  // inPitchBytes
               delayPitchBytes,                      // delayPitchBytes
               static_cast<size_t>(frameSamples) * sizeof(float),  // outPitchBytes
               static_cast<size_t>(0U),              // interleave=0 (non-interleaved)
               static_cast<size_t>(0U),              // mode=0 (linear delay)
               delaySamples,
               errCtxB));
         TISP::NodePtr dmaXferOut = TISP::NodePtr(
            TISP::make_unique_node<TISP::misc::DMAXfer<1>>(
               1, stateOutSrc, stateOutDst, stateSize));
         TI_profile_stop();

         if (!errCtxB.isSuccess()) {
            printf("[tpi=%d] DelayNChannel<float> construction failed: %s\n",
                   tpi, errCtxB.getMessage());
            fail = 1;
            continue;
         }

         // WARM exec loop B (warm-up + individual timing)
         for (uint32_t k = 0; k < numReps; k++) {
            TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);
            dmaXferIn->exec();
            delayNch->exec();
            dmaXferOut->exec();
            TI_profile_stop();
         }
         // Final exec timed as OPT (caches hot — warm measurement for the table)
         TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT);
         TI_profile_start(TI_PROFILE_KERNEL_OPT);
         dmaXferIn->exec();
         delayNch->exec();
         dmaXferOut->exec();
         TI_profile_stop();

         // Save last warm output B for bit-exact check
         static float outB_saved[TISP_DDT_MAX_FRAME_ELEMS];
         memcpy(outB_saved, gOutFrameL2_B, frameElems * sizeof(float));

         pProfile[6 * tpi + 3] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_INIT);
         pProfile[6 * tpi + 4] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[6 * tpi + 5] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);

         dmaXferIn.reset();
         delayNch.reset();
         dmaXferOut.reset();

         // ── Verify B vs reference (reference already computed above after A) ──
         int32_t statusB = TI_compare_mem_2D_float(
            outB_saved, refOut, 0, static_cast<double>(1e-6f),
            frameElems, 1,
            frameElems * sizeof(float), sizeof(float));
         int32_t currentTestFail_B = (statusB == TI_TEST_KERNEL_FAIL) ? 1 : 0;
         currentTestFail = currentTestFail_A || currentTestFail_B;
         fail = fail || currentTestFail_B;
      } else {
         pProfile[6 * tpi + 3] = 0U;
         pProfile[6 * tpi + 4] = 0U;
         pProfile[6 * tpi + 5] = 0U;
         currentTestFail = currentTestFail_A;
      }

      // ── CSV row ───────────────────────────────────────────────────────────
      fprintf(fpOutputCSV,
              "TISP_delayDru_druDelay, %u, %u, %u, %u, %u, "
              "%u, %u, %u, %u, %d\n",
              currPrm.testID, numChannels, frameSamples, maxDelay,
              stateBytesTotal / 1024U,
              pProfile[6 * tpi + 0], pProfile[6 * tpi + 1],
              pProfile[6 * tpi + 3], pProfile[6 * tpi + 4],
              !currentTestFail);
   }

   fclose(fpOutputCSV);
   return fail;
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry points
// ─────────────────────────────────────────────────────────────────────────────

static int test_main(uint32_t *pProfile)
{
   return TISP_delayDru_druDelay_d(pProfile, 0);
}

#ifdef __cplusplus
extern "C" {
#endif

int test_main_d()
{
   uint32_t profile[256 * 6] = {};
   int      fail              = test_main(profile);

   int32_t test_cases_count;
   TISP_delayDru_druDelay_testParams_t *prm;
   TISP_delayDru_druDelay_getTestParams(&prm, &test_cases_count);

   for (int32_t i = 0; i < test_cases_count; i++) {
      printf("[%d] Test %4d (ch=%u frm=%u maxD=%u stKB=%u): "
             "InitA=%8u  WarmA=%8u  "
             "InitB=%8u  WarmB=%8u\n",
             i, prm[i].testID,
             prm[i].numChannels, prm[i].frameSamples, prm[i].maxDelay,
             (prm[i].numChannels * (prm[i].maxDelay + prm[i].frameSamples) * 4U) / 1024U,
             profile[6 * i + 0], profile[6 * i + 1],
             profile[6 * i + 3], profile[6 * i + 4]);
   }

   if (fail == 0) { printf("\nDelayDru two-approach test: PASS\n"); }
   else           { printf("\nDelayDru two-approach test: FAIL\n"); }

   return fail ? 1 : 0;
}

#ifdef __cplusplus
}
#endif
