#include "TISP_dmaXfer_memory_map.hpp"
#include "TISP_dmaXfer_idat.h"
#include "TISP_test.h"

#include <cstring>
#include <cstdio>
#include <memory>
#include <stdint.h>
#include <tisp.hpp>
#include <TISP_dmaXfer.hpp>
#include <TISP_ErrorCtxt.hpp>
#if !defined(HOST_EMULATION)
#include <kernel/dpl/CacheP.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Natural-C reference
// ─────────────────────────────────────────────────────────────────────────────

static void dmaXfer_cn(const uint8_t *src, uint8_t *dst, uint32_t bytes)
{
   if (src != nullptr && dst != nullptr) {
      memcpy(dst, src, bytes);
   }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main test function (MATHLIB-style)
// ─────────────────────────────────────────────────────────────────────────────

static int TISP_dmaXfer_d(uint32_t *pProfile, uint8_t /*LevelOfFeedback*/)
{
   int32_t  currentTestFail;
   int32_t  fail     = 0;
   int32_t  test_cases;
   uint32_t testNum;
   char     desc[128];

   TISP_dmaXfer_testParams_t *prm;
   TISP_dmaXfer_testParams_t  currPrm;
   TISP_dmaXfer_getTestParams(&prm, &test_cases);

   TI_profile_init("TISP_dmaXfer");

   FILE *fpOutputCSV = fopen("TISP_dmaXfer.csv", "w+");
   fprintf(fpOutputCSV,
           "Kernel, Test ID, Mem, W, n, Total Bytes, Init Cycles, Warm Cycles, Pass/Fail\n");

   // CLEC / DRU initialisation — must happen once before any DMA transfer
#if defined(__C7100__) || defined(__C7120__)
   TISP::test_sciclientDmscGetVersion(NULL, 0);
   TISP::appC7xClecInitDru();
#elif !defined(HOST_EMULATION)
   App_dmautilsClecInitDru();
#endif

   // Single shared UDMA driver instance — avoids multiple Udma_init() calls
   // sharing the same CLEC/DRU resources across per-test-case constructions.
   // W=2 needs 3 channels (evict + 2 loads); size the context for the peak.
   TISP::ErrorCtxt rtErrCtx;
#if !defined(HOST_EMULATION)
   TISP::misc::DMARuntime rt(UDMA_INST_ID_0, 3, rtErrCtx);
#else
   TISP::misc::DMARuntime rt(0u, 3, rtErrCtx);
#endif
   if (!rtErrCtx.isSuccess()) {
      (void) printf("[Error] DMARuntime init failed: %s\n", rtErrCtx.getMessage());
      return 1;
   }
   static const int32_t chIds_w1[] = {0};
   static const int32_t chIds_w2[] = {0, 1, 2};

   for (int vi = 0; vi < N_EXT_VARIANTS; vi++) {
      uint8_t    *extBase = gExtVariants[vi].extBuf;
      const char *memTag  = gExtVariants[vi].name;

   for (int32_t tpi = 0; tpi < test_cases; tpi++) {
      int32_t  run_idx         = vi * test_cases + tpi;
      currPrm                  = prm[tpi];
      testNum                  = currPrm.testID;
      currentTestFail          = 0;

      // ── Seed sources + build golden refs for this case ────────────────────
      for (uint32_t t = 0; t < currPrm.n; t++) {
         uint32_t sz = currPrm.sizes[t];
         if (currPrm.dirs[t] == 'S') {
            // Store L2→ext: seed L2 source, clear ext dest, build store ref
            auto *l2Src  = reinterpret_cast<float *>(L2_ADDR(currPrm.l2Slot[t]));
            auto *extDst = reinterpret_cast<float *>(EXT_ADDR(extBase, currPrm.ddrSlot[t]));
            auto *ref    = reinterpret_cast<float *>(REF_STORE_EXT_ADDR(currPrm.ddrSlot[t]));
            uint32_t elems = sz / sizeof(float);
            for (uint32_t i = 0; i < elems; i++) {
               l2Src[i] = (float)(testNum * 10000 + t * 1000 + i);
            }
            memset(extDst, 0, sz);
            dmaXfer_cn(reinterpret_cast<uint8_t *>(l2Src),
                       reinterpret_cast<uint8_t *>(ref), sz);
         }
         else {
            // Load ext→L2: seed ext source, clear L2 dest, build load ref
            auto *extSrc = reinterpret_cast<float *>(EXT_ADDR(extBase, currPrm.ddrSlot[t]));
            auto *l2Dst  = reinterpret_cast<float *>(L2_ADDR(currPrm.l2Slot[t]));
            auto *ref    = reinterpret_cast<float *>(REF_LOAD_EXT_ADDR(currPrm.ddrSlot[t]));
            uint32_t elems = sz / sizeof(float);
            for (uint32_t i = 0; i < elems; i++) {
               extSrc[i] = (float)(50000 + testNum * 10000 + t * 1000 + i);
            }
            memset(l2Dst, 0, sz);
            dmaXfer_cn(reinterpret_cast<uint8_t *>(extSrc),
                       reinterpret_cast<uint8_t *>(ref), sz);
         }
      }

      // ── Materialise src/dst/size arrays from slot indices ─────────────────
      void  *srcAddrs[TISP_DMAXFER_MAX_TRANSFERS];
      void  *dstAddrs[TISP_DMAXFER_MAX_TRANSFERS];
      size_t sizes[TISP_DMAXFER_MAX_TRANSFERS];
      for (uint32_t t = 0; t < currPrm.n; t++) {
         if (currPrm.dirs[t] == 'S') {
            srcAddrs[t] = L2_ADDR(currPrm.l2Slot[t]);
            dstAddrs[t] = EXT_ADDR(extBase, currPrm.ddrSlot[t]);
         }
         else {
            srcAddrs[t] = EXT_ADDR(extBase, currPrm.ddrSlot[t]);
            dstAddrs[t] = L2_ADDR(currPrm.l2Slot[t]);
         }
         sizes[t] = currPrm.sizes[t];
      }

      // ── Write back CPU-seeded sources before DMA ──────────────────────────
      // Sources are seeded via cached CPU writes; the DRU bypasses L1D and reads
      // physical memory, so dirty source lines must be flushed first. Harmless for
      // non-cacheable regions (HyperRAM); required for cacheable OCRAM, otherwise
      // the DMA reads stale data left by a prior test case.
#if !defined(HOST_EMULATION)
      for (uint32_t t = 0; t < currPrm.n; t++) {
         (void) CacheP_wbInv(srcAddrs[t], (uint32_t)sizes[t], CacheP_TYPE_ALLD);
      }
#endif

      // ── Profile: init (node construction) ────────────────────────────────
      TISP::ErrorCtxt nodeErrCtx;
      TI_profile_start(TI_PROFILE_KERNEL_INIT);
      TISP::NodePtr k0 = (currPrm.w == 2U)
         ? TISP::NodePtr(TISP::make_unique_node<TISP::misc::DMAXfer<2>>(rt, 3, chIds_w2, (int32_t)currPrm.n, srcAddrs, dstAddrs, sizes, nodeErrCtx))
         : TISP::NodePtr(TISP::make_unique_node<TISP::misc::DMAXfer<1>>(rt, 1, chIds_w1, (int32_t)currPrm.n, srcAddrs, dstAddrs, sizes, nodeErrCtx));
      TI_profile_stop();
      if (!nodeErrCtx.isSuccess()) {
         (void) printf("[Error] DMAXfer<%u> init failed: %s\n", currPrm.w, nodeErrCtx.getMessage());
         currentTestFail = 1;
      }

      // ── Profile: cold exec ────────────────────────────────────────────────
      TI_profile_start(TI_PROFILE_KERNEL_OPT);
      k0->exec();
      TI_profile_stop();

      // ── Profile: warm exec loop ───────────────────────────────────────────
      for (uint32_t k = 0; k < (uint32_t)currPrm.numReps; k++) {
         TI_profile_clear_cycle_count_single(TI_PROFILE_KERNEL_OPT_WARM);
         TI_profile_start(TI_PROFILE_KERNEL_OPT_WARM);
         k0->exec();
         TI_profile_stop();
      }

      // ── k0 destroyed here → DMAXfer destructor → Udma_deinit ─────────────
      k0.reset();

      // ── Invalidate dst cache lines before CPU comparison ──────────────────
      // DMA bypasses L1D; test CPU reads destinations directly, so invalidate here.
#if !defined(HOST_EMULATION)
      for (uint32_t t = 0; t < currPrm.n; t++) {
         void *dst = (currPrm.dirs[t] == 'S')
            ? (void *)EXT_ADDR(extBase, currPrm.ddrSlot[t])
            : (void *)L2_ADDR(currPrm.l2Slot[t]);
         (void) CacheP_inv(dst, currPrm.sizes[t], CacheP_TYPE_ALLD);
      }
#endif

      // ── Compare output ────────────────────────────────────────────────────
      int32_t status = TI_TEST_KERNEL_PASS;
      for (uint32_t t = 0; t < currPrm.n; t++) {
         uint32_t elems = currPrm.sizes[t] / sizeof(float);
         void    *got;
         void    *ref;
         if (currPrm.dirs[t] == 'S') {
            got = EXT_ADDR(extBase, currPrm.ddrSlot[t]);
            ref = REF_STORE_EXT_ADDR(currPrm.ddrSlot[t]);
         }
         else {
            got = L2_ADDR(currPrm.l2Slot[t]);
            ref = REF_LOAD_EXT_ADDR(currPrm.ddrSlot[t]);
         }
         status &= TI_compare_mem_2D_float(
            got, ref, 0, static_cast<double>(1e-6f),
            elems, 1, currPrm.sizes[t], sizeof(float));
      }
      currentTestFail = (status == TI_TEST_KERNEL_FAIL) ? 1 : 0;
      fail            = fail || currentTestFail;

      // ── Record cycles ─────────────────────────────────────────────────────
      pProfile[3 * run_idx]     = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_INIT);
      pProfile[3 * run_idx + 1] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
      pProfile[3 * run_idx + 2] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);

      uint32_t totalBytes = 0;
      for (uint32_t t = 0; t < currPrm.n; t++) { totalBytes += currPrm.sizes[t]; }

      snprintf(desc, sizeof(desc), "W%u n=%u, %u B total [%s]",
               currPrm.w, currPrm.n, totalBytes, memTag);

      fprintf(fpOutputCSV,
              "TISP_dmaXfer_W%u_%s, %u, %s, %u, %u, %u, %u, %u, %d\n",
              currPrm.w, memTag, testNum, memTag, currPrm.w, currPrm.n, totalBytes,
              pProfile[3 * run_idx], pProfile[3 * run_idx + 1], pProfile[3 * run_idx + 2],
              !currentTestFail);

      TI_profile_add_test(testNum, (int)currPrm.n, 0, 0, currentTestFail, desc);
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
   return TISP_dmaXfer_d(pProfile, 0);
}

#ifdef __cplusplus
extern "C" {
#endif

int test_main_d()
{
   uint32_t profile[256 * 3] = {};
   int      fail              = test_main(profile);

   int32_t test_cases_count;
   TISP_dmaXfer_testParams_t *prm;
   TISP_dmaXfer_getTestParams(&prm, &test_cases_count);

   if (fail == 0) { printf("\nTest Pass!\n"); }
   else           { printf("\nTest FAIL!\n"); }

   for (int vi = 0; vi < N_EXT_VARIANTS; vi++) {
      const char *memTag = gExtVariants[vi].name;
      for (int32_t i = 0; i < test_cases_count; i++) {
         int32_t run_idx = vi * test_cases_count + i;
         printf("[%s] Test %4d: Init=%8u  Cold=%8u  Warm=%8u\n",
                memTag, prm[i].testID,
                profile[3 * run_idx], profile[3 * run_idx + 1], profile[3 * run_idx + 2]);
      }
   }

   return fail ? 1 : 0;
}

#ifdef __cplusplus
}
#endif
