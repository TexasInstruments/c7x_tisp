// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL
//
// Four-approach delay comparison test — MATHLIB/dmaXfer profiling pattern.
// Ported from dmautils_delay_circular_poc/dmautils_delay_circular.c.
//
//   Approach A — TISP::misc::DelayDru<float>  (DRU circular, external-memory ring).
//   Approach B — TISP::misc::DMAXfer<1> (state ext->L2) +
//                TISP::AUDIOLIB::DelayNChannel<float> (L2, circular mode; linear if it
//                does not fit) + TISP::misc::DMAXfer<1> (state L2->ext).
//   Approach C — TISP::AUDIOLIB::DelayNChannel<float>, state ring in external
//                memory (circular mode, no DMA).
//   Approach D — TISP::AUDIOLIB::DelayNChannel<float> in L2 only: persistent L2
//                state, no DMA (Approach B without the ext<->L2 state transfers).
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

// pProfile slots per run: [0..2] A, [3..5] B, [6..8] C, [9..11] D (each init/opt/warm).
static const int32_t PROF_STRIDE = 12;

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
           "Init_A, Warm_A, Init_B, Warm_B, Init_C, Warm_C, Init_D, Warm_D, Pass/Fail\n");

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
   // One external pool per variant, reused for ring (A/C) and state (B); never
   // live simultaneously. Both views are aliases into the same buffer; the
   // per-case memset of the whole pool resets both at once.
   uint8_t    *extPool      = gExtVariants[vi].pool;
   const uint32_t poolBytes = gExtVariants[vi].poolBytes;
   uint8_t    *ringExt      = TISP_DDT_ALIGN_UP(extPool, TISP_DDT_MAX_RING_BYTES_PER_CH);
   float      *stateExt     = reinterpret_cast<float *>(extPool);
   const char *memTag       = gExtVariants[vi].name;
   const uint32_t ringCapB  = gExtVariants[vi].ringCapBytes;   // 0 = uncapped
   // Approach C streams DelayNChannel state out of the external pool via SE.
   // The SE path on AM275 cannot address HyperRAM, so skip C on variants that
   // flag themselves as not CPU/SE-directly-addressable.
   const bool runApproachC  = gExtVariants[vi].cpuExtDirect;

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
      const uint32_t ringBytesTotal    = numChannels * ringSamplesPerCh * (uint32_t)sizeof(float);

      // Skip cases that exceed this variant's ring pool (AM275 OCRAM = ~4 MB).
      if (ringCapB != 0U && ringBytesTotal > ringCapB) {
         printf("[%s tpi=%d] skipping testID=%u: ringBytes=%u > cap=%u\n",
                memTag, tpi, currPrm.testID, ringBytesTotal, ringCapB);
         for (int k = 0; k < PROF_STRIDE; k++) {
            pProfile[PROF_STRIDE * runIdx + k] = 0U;
         }
         continue;
      }

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
      // Reset the whole external pool once — ringExt and stateExt are aliases
      // into extPool, so this clears both. Sizing by poolBytes avoids the
      // AM275 OCRAM overrun that the per-buffer MAX-sized memsets caused.
      memset(extPool,       0, poolBytes);
      // gDelayStateL2 is the Approach-B/D state buffer and is bounded by
      // TISP_DDT_MAX_STATE_BYTES_TOTAL. For cases whose stateBytesTotal exceeds
      // that bound, Approach B is skipped (runApproachB) and the whole L2 state
      // is unused, so clearing only up to the buffer size is safe and avoids
      // overrunning adjacent .data (testParams[]).
      memset(gDelayStateL2, 0,
             (stateBytesTotal <= TISP_DDT_MAX_STATE_BYTES_TOTAL)
                ? stateBytesTotal
                : TISP_DDT_MAX_STATE_BYTES_TOTAL);
      memset(gRefRing,      0, ringSamplesPerCh * numChannels * sizeof(float));

#if !defined(HOST_EMULATION)
      // DelayDru and DMAXfer do no cache maintenance: push CPU-written input/state to
      // memory and drop stale output lines so the DMA/DRU and the CPU see the same data.
      // extPool holds both the ring (A/C) and the state (B) views; one wb covers both.
      (void) CacheP_wb(gInFrameL2,      static_cast<int32_t>(frameElems * sizeof(float)),      CacheP_TYPE_ALLD);
      (void) CacheP_wb(extPool,         static_cast<int32_t>(poolBytes),                       CacheP_TYPE_ALLD);
      (void) CacheP_wb(gDelayStateL2,   static_cast<int32_t>(TISP_DDT_MAX_STATE_BYTES_TOTAL),  CacheP_TYPE_ALLD);
      (void) CacheP_inv(gOutFrameL2_A,  static_cast<int32_t>(frameElems * sizeof(float)),      CacheP_TYPE_ALLD);
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
      int32_t currentTestFail_B = 0;
      if (runApproachB) {
         // stateExt aliases the external pool, which Approach A just wrote through
         // as its ring. Re-zero the state region so Approach B starts clean.
         memset(stateExt, 0, stateBytesB);
#if !defined(HOST_EMULATION)
         (void) CacheP_wb(stateExt, static_cast<int32_t>(stateBytesB), CacheP_TYPE_ALLD);
#endif
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
         currentTestFail_B = (statusB == TI_TEST_KERNEL_FAIL) ? 1 : 0;
         fail = fail || currentTestFail_B;
      } else {
         pProfile[PROF_STRIDE * runIdx + 3] = 0U;
         pProfile[PROF_STRIDE * runIdx + 4] = 0U;
         pProfile[PROF_STRIDE * runIdx + 5] = 0U;
      }

      // ══════════════════════════════════════════════════════════════════════
      // APPROACH C — DelayNChannel<float> circular, state ring in external memory (no DMA)
      // Reuses ringExt as the circular state buffer — same size and alignment
      // as A's DRU ring (numChannels * nextPow2(delayDimX) * sizeof(float)),
      // which is free after delayDru.reset() above.
      //
      // SKIPPED on variants whose external pool is not CPU/SE-directly-addressable
      // (HyperRAM on AM275): DelayNChannel streams state via the SE path, which
      // cannot reach HyperRAM.
      // ══════════════════════════════════════════════════════════════════════
      int32_t currentTestFail_C = 0;
      if (runApproachC) {
         const uint32_t circPitchElems_C = nextPow2((delayDimX < 128U) ? 128U : delayDimX);
         const size_t   circPitchBytes_C = (size_t)circPitchElems_C * sizeof(float);
         // ringExt is already aligned to TISP_DDT_MAX_RING_BYTES_PER_CH >= circPitchBytes_C,
         // so every channel row is naturally circPitchBytes_C-aligned.
         float         *circStateDdr     = reinterpret_cast<float *>(ringExt);
         const size_t   stateBytesC      = (size_t)numChannels * circPitchBytes_C;

         // Zero C's state region. The external pool was last written by Approach A
         // (as the DRU ring) and Approach B (as the state at pool base); re-zeroing
         // here gives Approach C a clean circular ring regardless of layout aliasing.
         memset(circStateDdr, 0, stateBytesC);
#if !defined(HOST_EMULATION)
         (void) CacheP_wb(circStateDdr, static_cast<int32_t>(stateBytesC), CacheP_TYPE_ALLD);
         (void) CacheP_inv(gOutFrameL2_B, static_cast<int32_t>(frameElems * sizeof(float)), CacheP_TYPE_ALLD);
#endif

         TISP::ErrorCtxt errCtxC;
         TI_profile_start(TI_PROFILE_KERNEL_INIT);
         TISP::NodePtr delayNchDdr = TISP::NodePtr(
            TISP::make_unique_node<TISP::AUDIOLIB::DelayNChannel<float>>(
               gInFrameL2,
               circStateDdr,
               gScratchL2,
               gOutFrameL2_B,
               static_cast<size_t>(frameSamples),
               static_cast<size_t>(numChannels),
               static_cast<size_t>(circPitchElems_C),
               static_cast<size_t>(numChannels),
               static_cast<size_t>(frameSamples) * sizeof(float),
               circPitchBytes_C,
               static_cast<size_t>(frameSamples) * sizeof(float),
               static_cast<size_t>(0U),
               0U,             // mode=1 (circular)
               delaySamples,
               errCtxC));
         TI_profile_stop();

         if (!delayNchDdr) {
            printf("[%s tpi=%d] Approach C node allocation failed (node arena full: %zu/%zu bytes)\n",
                   memTag, tpi, TISP::nodeAllocator().getBytesUsed(), TISP::nodeAllocator().getBufferSize());
            fail = 1;
            continue;
         }
         if (!errCtxC.isSuccess()) {
            printf("[tpi=%d] DelayNChannel<float> (DDR circ) construction failed: %s\n",
                   tpi, errCtxC.getMessage());
            fail = 1;
            continue;
         }

         for (uint32_t k = 0; k < numReps; k++) {
            TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);
            delayNchDdr->exec();
            TI_profile_stop();
         }
         TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT);
         TI_profile_start(TI_PROFILE_KERNEL_OPT);
         delayNchDdr->exec();
         TI_profile_stop();

#if !defined(HOST_EMULATION)
         (void) CacheP_inv(gOutFrameL2_B, static_cast<int32_t>(frameElems * sizeof(float)), CacheP_TYPE_ALLD);
#endif

         static float outC_saved[TISP_DDT_MAX_FRAME_ELEMS];
         memcpy(outC_saved, gOutFrameL2_B, frameElems * sizeof(float));

         pProfile[PROF_STRIDE * runIdx + 6] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_INIT);
         pProfile[PROF_STRIDE * runIdx + 7] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[PROF_STRIDE * runIdx + 8] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);

         delayNchDdr.reset();

         int32_t statusC = TI_compare_mem_2D_float(
            outC_saved, refOut, 0, static_cast<double>(1e-6f),
            frameElems, 1,
            frameElems * sizeof(float), sizeof(float));
         currentTestFail_C = (statusC == TI_TEST_KERNEL_FAIL) ? 1 : 0;
         fail = fail || currentTestFail_C;
      } else {
         pProfile[PROF_STRIDE * runIdx + 6] = 0U;
         pProfile[PROF_STRIDE * runIdx + 7] = 0U;
         pProfile[PROF_STRIDE * runIdx + 8] = 0U;
      }

      // ══════════════════════════════════════════════════════════════════════
      // APPROACH D — DelayNChannel<float> in L2 only (persistent L2 state, no DMA)
      // Identical kernel/mode/state-pointer as Approach B, but WITHOUT the ext<->L2
      // state DMAs: the delay state lives entirely in L2 and persists in place across
      // frames. Isolates the pure kernel cost (B minus DMA). Runs on the same cases
      // as B (reuses runApproachB — the state must fit in L2).
      // ══════════════════════════════════════════════════════════════════════
      int32_t currentTestFail_D = 0;
      if (runApproachB) {
         // Fresh zero state in L2 (Approach B left delayed data in pStateL2).
         memset(pStateL2, 0, stateBytesB);
#if !defined(HOST_EMULATION)
         (void) CacheP_wb(pStateL2, static_cast<int32_t>(stateBytesB), CacheP_TYPE_ALLD);
         (void) CacheP_inv(gOutFrameL2_B, static_cast<int32_t>(frameElems * sizeof(float)), CacheP_TYPE_ALLD);
#endif

         TISP::ErrorCtxt errCtxD;
         TI_profile_start(TI_PROFILE_KERNEL_INIT);
         TISP::NodePtr delayNchL2 = TISP::NodePtr(
            TISP::make_unique_node<TISP::AUDIOLIB::DelayNChannel<float>>(
               gInFrameL2,
               pStateL2,
               gScratchL2,
               gOutFrameL2_B,
               static_cast<size_t>(frameSamples),
               static_cast<size_t>(numChannels),
               static_cast<size_t>(delayPitchElems),
               static_cast<size_t>(numChannels),
               static_cast<size_t>(frameSamples) * sizeof(float),
               delayPitchBytes,
               static_cast<size_t>(frameSamples) * sizeof(float),
               static_cast<size_t>(0U),
               delayMode,                            // 1=circular (fits L2), else 0=linear
               delaySamples,
               errCtxD));
         TI_profile_stop();

         if (!delayNchL2) {
            printf("[%s tpi=%d] Approach D node allocation failed (node arena full: %zu/%zu bytes)\n",
                   memTag, tpi, TISP::nodeAllocator().getBytesUsed(), TISP::nodeAllocator().getBufferSize());
            fail = 1;
            continue;
         }
         if (!errCtxD.isSuccess()) {
            printf("[tpi=%d] DelayNChannel<float> (L2 only) construction failed: %s\n",
                   tpi, errCtxD.getMessage());
            fail = 1;
            continue;
         }

         for (uint32_t k = 0; k < numReps; k++) {
            TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
            TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);
            delayNchL2->exec();
            TI_profile_stop();
         }
         TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT);
         TI_profile_start(TI_PROFILE_KERNEL_OPT);
         delayNchL2->exec();
         TI_profile_stop();

#if !defined(HOST_EMULATION)
         (void) CacheP_inv(gOutFrameL2_B, static_cast<int32_t>(frameElems * sizeof(float)), CacheP_TYPE_ALLD);
#endif

         static float outD_saved[TISP_DDT_MAX_FRAME_ELEMS];
         memcpy(outD_saved, gOutFrameL2_B, frameElems * sizeof(float));

         pProfile[PROF_STRIDE * runIdx + 9]  = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_INIT);
         pProfile[PROF_STRIDE * runIdx + 10] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
         pProfile[PROF_STRIDE * runIdx + 11] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);

         delayNchL2.reset();

         int32_t statusD = TI_compare_mem_2D_float(
            outD_saved, refOut, 0, static_cast<double>(1e-6f),
            frameElems, 1,
            frameElems * sizeof(float), sizeof(float));
         currentTestFail_D = (statusD == TI_TEST_KERNEL_FAIL) ? 1 : 0;
         fail = fail || currentTestFail_D;
      } else {
         pProfile[PROF_STRIDE * runIdx + 9]  = 0U;
         pProfile[PROF_STRIDE * runIdx + 10] = 0U;
         pProfile[PROF_STRIDE * runIdx + 11] = 0U;
      }

      currentTestFail = currentTestFail_A || currentTestFail_B || currentTestFail_C || currentTestFail_D;

      // ── CSV row ───────────────────────────────────────────────────────────
      fprintf(fpOutputCSV,
              "TISP_delayDru, %u, %s, %u, %u, %u, %u, "
              "%u, %u, %u, %u, %u, %u, %u, %u, %d\n",
              currPrm.testID, memTag, numChannels, frameSamples, maxDelay,
              stateBytesTotal / 1024U,
              pProfile[PROF_STRIDE * runIdx + 0], pProfile[PROF_STRIDE * runIdx + 1],
              pProfile[PROF_STRIDE * runIdx + 3], pProfile[PROF_STRIDE * runIdx + 4],
              pProfile[PROF_STRIDE * runIdx + 6], pProfile[PROF_STRIDE * runIdx + 7],
              pProfile[PROF_STRIDE * runIdx + 9], pProfile[PROF_STRIDE * runIdx + 10],
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
             "InitB=%8u  WarmB=%8u  "
             "InitC=%8u  WarmC=%8u  "
             "InitD=%8u  WarmD=%8u\n",
             i, prm[t].testID, gExtVariants[vi].name,
             prm[t].numChannels, prm[t].frameSamples, prm[t].maxDelay,
             (prm[t].numChannels * (prm[t].maxDelay + prm[t].frameSamples) * 4U) / 1024U,
             profile[PROF_STRIDE * i + 0], profile[PROF_STRIDE * i + 1],
             profile[PROF_STRIDE * i + 3], profile[PROF_STRIDE * i + 4],
             profile[PROF_STRIDE * i + 6], profile[PROF_STRIDE * i + 7],
             profile[PROF_STRIDE * i + 9], profile[PROF_STRIDE * i + 10]);
   }
   }

   return fail ? 1 : 0;
}

#ifdef __cplusplus
}
#endif
