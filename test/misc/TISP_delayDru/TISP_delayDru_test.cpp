// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL
//
// Two-approach delay comparison test — MATHLIB/dmaXfer profiling pattern.
// Ported from dmautils_delay_circular_poc/dmautils_delay_circular.c.
//
//   Approach A — TISP::misc::DelayDru<float>  (DRU circular, external-memory ring).
//   Approach B — TISP::misc::DMAXfer<1> (state ext->L2) +
//                TISP::AUDIOLIB::DelayNChannel<float> (L2, circular mode; linear if it
//                does not fit) + TISP::misc::DMAXfer<1> (state L2->ext).
//
// "External memory" is DDR on AM62D. On AM275 the same test cases run twice:
// once with the ring/state in OCRAM (.ddrData maps to OCRAM there) and once in
// HyperRAM (.hyperRamData); see gExtVariants in the memory map.
//
// Each approach is profiled (INIT / COLD / WARM) per test case using the
// TI_profile_* API, exactly mirroring TISP_dmaXfer_test.cpp.  Both approaches
// are verified bit-exact against a ported CPU reference for every test case.
//
// DEVIATION FROM SPEC: on-target the ring (A) and persistent state (B) should
// live in a non-cacheable external region (.ddr_state_nc, MAIR4) for full DRU
// bandwidth.  The tisp repo has no such linker region; both buffers use the
// standard cacheable .ddrData section.  This does not affect the required
// HOST_EMULATION correctness gate.

#include "TISP_delayDru_memory_map.hpp"
#include "TISP_delayDru_idat.h"
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

// pProfile slots per run: [0..2] A init/opt/warm, [3..5] B init/opt/warm.
static const int32_t PROF_STRIDE = 6;

// ─────────────────────────────────────────────────────────────────────────────
// Main test function — MATHLIB/dmaXfer profiling pattern
// ─────────────────────────────────────────────────────────────────────────────

static int TISP_delayDru_d(uint32_t *pProfile,
                                        uint8_t  /*LevelOfFeedback*/)
{
   int32_t test_cases;
   TISP_delayDru_testParams_t *prm;
   TISP_delayDru_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_delayDru");

   FILE *fpOutputCSV = fopen("TISP_delayDru.csv", "w+");
   fprintf(fpOutputCSV,
           "Kernel, Test ID, Mem, numCh, frameSmp, maxDelay, statKB, "
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

   // One UDMA driver + one DmaUtils context (1 channel) shared by every node in every case:
   // DelayDru borrows the driver handle, the Approach-B DMAXfer nodes borrow the context and
   // reuse channel 0. Creating a driver/context per node leaks DRU channels across cases.
   TISP::ErrorCtxt dmaRtErrCtx;
#if !defined(HOST_EMULATION)
   TISP::misc::DMARuntime dmaRt(UDMA_INST_ID_0, 1, dmaRtErrCtx);
#else
   TISP::misc::DMARuntime dmaRt(0U, 1, dmaRtErrCtx);
#endif
   if (!dmaRtErrCtx.isSuccess()) {
      printf("DMARuntime init failed: %s\n", dmaRtErrCtx.getMessage());
      return 1;
   }
   const int32_t          dmaChIds[1] = { 0 };

   for (int vi = 0; vi < N_EXT_VARIANTS; vi++) {
   uint8_t    *ringExt  = TISP_DDT_ALIGN_UP(gExtVariants[vi].ring, TISP_DDT_MAX_RING_BYTES_PER_CH);
   float      *stateExt = gExtVariants[vi].state;
   const char *memTag   = gExtVariants[vi].name;

   for (int32_t tpi = 0; tpi < test_cases; tpi++) {
      const int32_t runIdx = vi * test_cases + tpi;
      TISP_delayDru_testParams_t currPrm = prm[tpi];
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
      memset(ringExt,        0, TISP_DDT_MAX_RING_BYTES_TOTAL);
      memset(stateExt,       0, TISP_DDT_MAX_STATE_BYTES_TOTAL);
      memset(gDelayStateL2,  0, TISP_DDT_MAX_STATE_BYTES_TOTAL);
      memset(gRefRing,       0, TISP_DDT_MAX_RING_BYTES_TOTAL);

#if !defined(HOST_EMULATION)
      // DelayDru and DMAXfer do no cache maintenance: push CPU-written input/state to
      // memory and drop stale output lines so the DMA/DRU and the CPU see the same data.
      (void) CacheP_wb(gInFrameL2,      static_cast<int32_t>(frameElems * sizeof(float)), CacheP_TYPE_ALLD);
      (void) CacheP_wb(stateExt,       static_cast<int32_t>(TISP_DDT_MAX_STATE_BYTES_TOTAL), CacheP_TYPE_ALLD);
      (void) CacheP_wb(gDelayStateL2,   static_cast<int32_t>(TISP_DDT_MAX_STATE_BYTES_TOTAL), CacheP_TYPE_ALLD);
      (void) CacheP_inv(gOutFrameL2_A,  static_cast<int32_t>(frameElems * sizeof(float)), CacheP_TYPE_ALLD);
#endif

      // ══════════════════════════════════════════════════════════════════════
      // APPROACH A — DelayDru<float> (DRU circular external-memory ring)
      // ══════════════════════════════════════════════════════════════════════

      // UDMA driver is application-owned: shared from dmaRt for every case.
      void *pUdmaDrvHandle = nullptr;
#if !defined(HOST_EMULATION)
      pUdmaDrvHandle = dmaRt.drvHandle();
#endif

      // INIT: node construction
      TISP::ErrorCtxt errCtxA;
      TI_profile_start(TI_PROFILE_KERNEL_INIT);
      TISP::NodePtr delayDru = TISP::NodePtr(
         TISP::make_unique_node<TISP::misc::DelayDru<float>>(
            gInFrameL2, gOutFrameL2_A,
            static_cast<void *>(ringExt),
            delaySamples,
            static_cast<size_t>(frameSamples),
            static_cast<size_t>(numChannels),
            pUdmaDrvHandle,
            errCtxA));
      TI_profile_stop();

      if (!delayDru) {
         printf("[%s tpi=%d] DelayDru node allocation failed (node arena full: %zu/%zu bytes)\n",
                memTag, tpi, TISP::nodeAllocator().getBytesUsed(), TISP::nodeAllocator().getBufferSize());
         fail = 1;
         continue;
      }
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

#if !defined(HOST_EMULATION)
      // DRU wrote gOutFrameL2_A behind the CPU cache; invalidate before validating.
      (void) CacheP_inv(gOutFrameL2_A, static_cast<int32_t>(frameElems * sizeof(float)), CacheP_TYPE_ALLD);
#endif

      // Save last warm output A for bit-exact check
      static float outA_saved[TISP_DDT_MAX_FRAME_ELEMS];
      memcpy(outA_saved, gOutFrameL2_A, frameElems * sizeof(float));

      pProfile[PROF_STRIDE * runIdx + 0] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_INIT);
      pProfile[PROF_STRIDE * runIdx + 1] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
      pProfile[PROF_STRIDE * runIdx + 2] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);

      delayDru.reset();  // DelayDru destructor (closes its DRU channel; driver stays with dmaRt)

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
      snprintf(desc, sizeof(desc), "%s ch=%u frm=%u maxD=%u stKB=%u [A]",
               memTag, numChannels, frameSamples, maxDelay, stateBytesTotal / 1024U);
      // TI_PROFILE_KERNEL_* buckets still hold Approach A values here
      TI_profile_add_test(currPrm.testID, (int)frameElems, 0, 0,
                          currentTestFail_A, desc);

      // ══════════════════════════════════════════════════════════════════════
      // APPROACH B — DMAXfer<1> (state ext->L2) + DelayNChannel + DMAXfer<1>
      // ══════════════════════════════════════════════════════════════════════
      // External state is already zero (cleared above); L2 state is also zero.
      // DMAXfer addresses are local but DMAXfer<1> makes heap copies. Both DMAXfer nodes borrow
      // dmaRt and run serially on its channel 0.

      // DelayNChannel circular mode (mode=1) reads each delay row with an SE CIRC_CBK0 stream, which
      // wraps inside a power-of-two block (>= 512 B) aligned to its own size, while the write side
      // wraps at the row pitch. So the pitch must be exactly a power of two and every row must start
      // on a pitch boundary, else reads run into / wrap onto a neighbouring channel's row. With a
      // power-of-two pitch, aligning the buffer base aligns every row.
      const uint32_t circPitchElems = nextPow2((delayDimX < 128U) ? 128U : delayDimX);
      const size_t   circPitchBytes = (size_t)circPitchElems * sizeof(float);
      float         *circStateL2    = reinterpret_cast<float *>(TISP_DDT_ALIGN_UP(gDelayStateL2, circPitchBytes));
      const size_t   circAlignPad   = static_cast<size_t>(reinterpret_cast<uint8_t *>(circStateL2) -
                                                          reinterpret_cast<uint8_t *>(gDelayStateL2));
      const bool     useCircular    =
         (circAlignPad + numChannels * circPitchBytes) <= TISP_DDT_MAX_STATE_BYTES_TOTAL;

      // Circular whenever the padded, aligned state fits in L2; otherwise fall back to linear (mode=0),
      // which has no pitch/alignment constraint.
      const size_t   delayMode       = useCircular ? 1U : 0U;
      const uint32_t delayPitchElems = useCircular ? circPitchElems : delayDimX;
      const size_t   delayPitchBytes = (size_t)delayPitchElems * sizeof(float);
      float         *pStateL2        = useCircular ? circStateL2 : gDelayStateL2;
      const size_t   stateBytesB     = (size_t)numChannels * delayPitchBytes;

      const bool runApproachB = useCircular || (stateBytesTotal <= APPROACH_B_MAX_STATE_BYTES);
      if (runApproachB) {
         void  *stateInSrc[1]  = { static_cast<void *>(stateExt) };
         void  *stateInDst[1]  = { static_cast<void *>(pStateL2) };
         void  *stateOutSrc[1] = { static_cast<void *>(pStateL2) };
         void  *stateOutDst[1] = { static_cast<void *>(stateExt) };
         size_t stateSize[1]   = { stateBytesB };

         // INIT B: construct all three B nodes
         TISP::ErrorCtxt errCtxB;
         TI_profile_start(TI_PROFILE_KERNEL_INIT);
         TISP::NodePtr dmaXferIn = TISP::NodePtr(
            TISP::make_unique_node<TISP::misc::DMAXfer<1>>(
               dmaRt, 1, dmaChIds, 1, stateInSrc, stateInDst, stateSize, errCtxB));
         TISP::NodePtr delayNch = TISP::NodePtr(
            TISP::make_unique_node<TISP::AUDIOLIB::DelayNChannel<float>>(
               gInFrameL2,
               pStateL2,
               gScratchL2,
               gOutFrameL2_B,
               static_cast<size_t>(frameSamples),   // width
               static_cast<size_t>(numChannels),     // height
               static_cast<size_t>(delayPitchElems), // widthD (whole row is live in circular mode)
               static_cast<size_t>(numChannels),     // heightD
               static_cast<size_t>(frameSamples) * sizeof(float),  // inPitchBytes
               delayPitchBytes,                      // delayPitchBytes (pow2 + aligned in circular)
               static_cast<size_t>(frameSamples) * sizeof(float),  // outPitchBytes
               static_cast<size_t>(0U),              // interleave=0 (non-interleaved)
               delayMode,                            // mode: 1=circular, 0=linear if it doesn't fit
               delaySamples,
               errCtxB));
         TISP::NodePtr dmaXferOut = TISP::NodePtr(
            TISP::make_unique_node<TISP::misc::DMAXfer<1>>(
               dmaRt, 1, dmaChIds, 1, stateOutSrc, stateOutDst, stateSize, errCtxB));
         TI_profile_stop();

         // make_unique_node returns null (without touching errCtx) when the node arena is full.
         if (!dmaXferIn || !delayNch || !dmaXferOut) {
            printf("[%s tpi=%d] Approach B node allocation failed (node arena full: %zu/%zu bytes)\n",
                   memTag, tpi, TISP::nodeAllocator().getBytesUsed(), TISP::nodeAllocator().getBufferSize());
            fail = 1;
            continue;
         }
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

         pProfile[PROF_STRIDE * runIdx + 3] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_INIT);
         pProfile[PROF_STRIDE * runIdx + 4] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[PROF_STRIDE * runIdx + 5] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);

         // Release in reverse construction order: the node arena is a bump allocator and only
         // reclaims the most recent block, so FIFO release leaks dmaXferIn + delayNch every case.
         dmaXferOut.reset();
         delayNch.reset();
         dmaXferIn.reset();

         // ── Verify B vs reference (reference already computed above after A) ──
         int32_t statusB = TI_compare_mem_2D_float(
            outB_saved, refOut, 0, static_cast<double>(1e-6f),
            frameElems, 1,
            frameElems * sizeof(float), sizeof(float));
         int32_t currentTestFail_B = (statusB == TI_TEST_KERNEL_FAIL) ? 1 : 0;
         currentTestFail = currentTestFail_A || currentTestFail_B;
         fail = fail || currentTestFail_B;
      } else {
         pProfile[PROF_STRIDE * runIdx + 3] = 0U;
         pProfile[PROF_STRIDE * runIdx + 4] = 0U;
         pProfile[PROF_STRIDE * runIdx + 5] = 0U;
         currentTestFail = currentTestFail_A;
      }

      // ── CSV row ───────────────────────────────────────────────────────────
      fprintf(fpOutputCSV,
              "TISP_delayDru, %u, %s, %u, %u, %u, %u, "
              "%u, %u, %u, %u, %d\n",
              currPrm.testID, memTag, numChannels, frameSamples, maxDelay,
              stateBytesTotal / 1024U,
              pProfile[PROF_STRIDE * runIdx + 0], pProfile[PROF_STRIDE * runIdx + 1],
              pProfile[PROF_STRIDE * runIdx + 3], pProfile[PROF_STRIDE * runIdx + 4],
              !currentTestFail);
   }
   } // vi

   fclose(fpOutputCSV);
   return fail;
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry points
// ─────────────────────────────────────────────────────────────────────────────

static int test_main(uint32_t *pProfile)
{
   return TISP_delayDru_d(pProfile, 0);
}

#ifdef __cplusplus
extern "C" {
#endif

int test_main_d()
{
   uint32_t profile[256 * PROF_STRIDE] = {};
   int      fail              = test_main(profile);

   int32_t test_cases_count;
   TISP_delayDru_testParams_t *prm;
   TISP_delayDru_getTestParams(&prm, &test_cases_count);

   if (fail == 0) { printf("\nTest Pass!\n"); }
   else           { printf("\nTest FAIL!\n"); }

   for (int vi = 0; vi < N_EXT_VARIANTS; vi++) {
   for (int32_t t = 0; t < test_cases_count; t++) {
      const int32_t i = vi * test_cases_count + t;
      printf("[%d] Test %4d [%s] (ch=%u frm=%u maxD=%u stKB=%u): "
             "InitA=%8u  WarmA=%8u  "
             "InitB=%8u  WarmB=%8u\n",
             i, prm[t].testID, gExtVariants[vi].name,
             prm[t].numChannels, prm[t].frameSamples, prm[t].maxDelay,
             (prm[t].numChannels * (prm[t].maxDelay + prm[t].frameSamples) * 4U) / 1024U,
             profile[PROF_STRIDE * i + 0], profile[PROF_STRIDE * i + 1],
             profile[PROF_STRIDE * i + 3], profile[PROF_STRIDE * i + 4]);
   }
   }

   return fail ? 1 : 0;
}

#ifdef __cplusplus
}
#endif
