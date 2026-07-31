#include "TISP_dmaXfer_memory_map.hpp"
#include "TISP_dmaXfer_idat.h"
#include "TISP_test.h"

#include <cstring>
#include <cstdio>
#include <memory>
#include <stdint.h>
#include <tisp.hpp>
#include <TISP_dmaXfer.hpp>
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
           "Kernel, Test ID, W, n, Total Bytes, Init Cycles, Warm Cycles, Pass/Fail\n");

   // CLEC / DRU initialisation — must happen once before any DMA transfer
#if defined(__C7100__) || defined(__C7120__)
   TISP::test_sciclientDmscGetVersion(NULL, 0);
   TISP::appC7xClecInitDru();
#elif !defined(HOST_EMULATION)
   App_dmautilsClecInitDru();
#endif

   for (int32_t tpi = 0; tpi < test_cases; tpi++) {
      currPrm          = prm[tpi];
      testNum          = currPrm.testID;
      currentTestFail  = 0;

      // ── Seed sources + build golden refs for this case ────────────────────
      for (uint32_t t = 0; t < currPrm.n; t++) {
         uint32_t sz = currPrm.sizes[t];
         if (currPrm.dirs[t] == 'S') {
            // Store L2→DDR: seed L2 source, clear DDR dest, build store ref
            auto *l2Src  = reinterpret_cast<float *>(L2_ADDR(currPrm.l2Slot[t]));
            auto *ddrDst = reinterpret_cast<float *>(DDR_ADDR(currPrm.ddrSlot[t]));
            auto *ref    = reinterpret_cast<float *>(REF_STORE_ADDR(currPrm.ddrSlot[t]));
            uint32_t elems = sz / sizeof(float);
            for (uint32_t i = 0; i < elems; i++) {
               l2Src[i] = (float)(testNum * 10000 + t * 1000 + i);
            }
            memset(ddrDst, 0, sz);
            dmaXfer_cn(reinterpret_cast<uint8_t *>(l2Src),
                       reinterpret_cast<uint8_t *>(ref), sz);
         }
         else {
            // Load DDR→L2: seed DDR source, clear L2 dest, build load ref
            auto *ddrSrc = reinterpret_cast<float *>(DDR_ADDR(currPrm.ddrSlot[t]));
            auto *l2Dst  = reinterpret_cast<float *>(L2_ADDR(currPrm.l2Slot[t]));
            auto *ref    = reinterpret_cast<float *>(REF_LOAD_ADDR(currPrm.ddrSlot[t]));
            uint32_t elems = sz / sizeof(float);
            for (uint32_t i = 0; i < elems; i++) {
               ddrSrc[i] = (float)(50000 + testNum * 10000 + t * 1000 + i);
            }
            memset(l2Dst, 0, sz);
            dmaXfer_cn(reinterpret_cast<uint8_t *>(ddrSrc),
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
            dstAddrs[t] = DDR_ADDR(currPrm.ddrSlot[t]);
         }
         else {
            srcAddrs[t] = DDR_ADDR(currPrm.ddrSlot[t]);
            dstAddrs[t] = L2_ADDR(currPrm.l2Slot[t]);
         }
         sizes[t] = currPrm.sizes[t];
      }

      // ── Profile: init (node construction) ────────────────────────────────
      TI_profile_start(TI_PROFILE_KERNEL_INIT);
      TISP::NodePtr k0 = (currPrm.w == 2U)
         ? TISP::NodePtr(TISP::make_unique_node<TISP::misc::DMAXfer<2>>((int32_t)currPrm.n, srcAddrs, dstAddrs, sizes))
         : TISP::NodePtr(TISP::make_unique_node<TISP::misc::DMAXfer<1>>((int32_t)currPrm.n, srcAddrs, dstAddrs, sizes));
      TI_profile_stop();

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
            ? (void *)DDR_ADDR(currPrm.ddrSlot[t])
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
            got = DDR_ADDR(currPrm.ddrSlot[t]);
            ref = REF_STORE_ADDR(currPrm.ddrSlot[t]);
         }
         else {
            got = L2_ADDR(currPrm.l2Slot[t]);
            ref = REF_LOAD_ADDR(currPrm.ddrSlot[t]);
         }
         status &= TI_compare_mem_2D_float(
            got, ref, 0, static_cast<double>(1e-6f),
            elems, 1, currPrm.sizes[t], sizeof(float));
      }
      currentTestFail = (status == TI_TEST_KERNEL_FAIL) ? 1 : 0;
      fail            = fail || currentTestFail;

      // ── Record cycles ─────────────────────────────────────────────────────
      pProfile[3 * tpi]     = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_INIT);
      pProfile[3 * tpi + 1] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT);
      pProfile[3 * tpi + 2] = (uint32_t)TI_profile_get_cycles(TI_PROFILE_KERNEL_OPT_WARM);

      uint32_t totalBytes = 0;
      for (uint32_t t = 0; t < currPrm.n; t++) { totalBytes += currPrm.sizes[t]; }

      snprintf(desc, sizeof(desc), "W%u n=%u, %u B total", currPrm.w, currPrm.n, totalBytes);

      fprintf(fpOutputCSV,
              "TISP_dmaXfer_W%u, %u, %u, %u, %u, %u, %u, %d\n",
              currPrm.w, testNum, currPrm.w, currPrm.n, totalBytes,
              pProfile[3 * tpi], pProfile[3 * tpi + 1], pProfile[3 * tpi + 2], !currentTestFail);

      TI_profile_add_test(testNum, (int)currPrm.n, 0, 0, currentTestFail, desc);
   }

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

   for (int32_t i = 0; i < test_cases_count; i++) {
      printf("Test %4d: Init=%8u  Cold=%8u  Warm=%8u\n",
             prm[i].testID, profile[3 * i], profile[3 * i + 1], profile[3 * i + 2]);
   }

   if (fail == 0) { printf("\nDMAXfer test: PASS\n"); }
   else           { printf("\nDMAXfer test: FAIL\n"); }

   return fail ? 1 : 0;
}

#ifdef __cplusplus
}
#endif
