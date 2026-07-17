// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/*!
 * \file TISP_utility.c
 */
/*******************************************************************************
 *
 * INCLUDES
 *
 ******************************************************************************/
//#include "TISP_test.h"
//
//#if !defined(_HOST_BUILD)
// extern const uint64_t pte_lvl0[512];
//#endif
//
//#ifdef __cplusplus
// extern "C" {
//#endif
//
// void TISP_TEST_init()
//{
//
//#if __C7X_VEC_SIZE_BITS__ == 512
//   // Known silicon bug, errata: TBD
//   __sa_set_cr(0, __sa_get_cr(1));
//#endif
//
//#if !defined(_HOST_BUILD)
//   // enable MMU
//   enable_cache_mmu((uint64_t) pte_lvl0);
//
//   // invalidate TLB
//   invalidate_tlb();
//#endif
//
//#ifndef TISP_FIXED_SEED
//   time_t t;
//   time(&t);
//   TISP_DEBUGPRINTFN(1, "rand seed = %u\n", (unsigned) t);
//   srand((unsigned) t);
//#else
//   TISP_DEBUGPRINTFN(1, "fixed seed = %u\n", (unsigned) TISP_FIXED_SEED);
//   srand((unsigned) TISP_FIXED_SEED);
//#endif
//
//#if !defined(DEMO)
//#if defined(_HOST_BUILD)
//#if defined(TISP_DEBUGPRINT)
//   mma::set_debug_level(TISP_DEBUGPRINT);
//   streaming_engine::set_debug_level(TISP_DEBUGPRINT);
//   sa_generator::set_debug_level(TISP_DEBUGPRINT);
//#endif
//#endif
//#endif
//}
//
//#ifdef __cplusplus
//}
//#endif
//
///* extern const uint64_t pte_lvl0[512]; */
///* #ifdef __cplusplus */
///* extern "C" { */
///* #endif */
///* void TISP_TEST_init() */
///* { */
///*    // enable MMU */
///*    enable_cache_mmu((uint64_t) pte_lvl0); */
//
///*    // invalidate TLB */
///*    invalidate_tlb(); */
//
///* // invalidate_tlb L1D */
///* #if !defined(_HOST_BUILD) */
//
///*    /\* printf ("\nInvalidating L1P Cache\n"); *\/ */
///*    TISP_c7xL1PCacheInvalidateAll(); */
///*    /\* printf ("\nInvalidating L1D Cache\n"); *\/ */
///*    TISP_c7xCleaninvalidateL1DCache(); */
///*    TISP_c7xInvalidateL1DCache(); */
//
///* #endif */
//
///*    // initialize the MMA */
///*    init_mma(); */
//
///* #ifndef TISP_FIXED_SEED */
///*    time_t t; */
///*    time(&t); */
///*    /\* TISP_DEBUGPRINTFN (1, "rand seed = %u\n", (unsigned) t); *\/ */
///*    /\* srand ((unsigned) t); *\/ */
///* #else */
///*    /\* TISP_DEBUGPRINTFN (1, "fixed seed = %u\n", (unsigned) TISP_FIXED_SEED); *\/ */
///*    /\* srand ((unsigned) TISP_FIXED_SEED); *\/ */
///* #endif */
//
///*    //#if !defined(_HOST_BUILD) */
///*    //   if (TISP_UTIL_mma_bist() ) { */
///*    //      printf("c7x_mma_bist failed.\n"); */
///*    //   } else { */
///*    //      printf("c7x_mma_bist passed.\n"); */
///*    //   } */
///*    //#endif */
//
///* #if !defined(DEMO) */
///* #if defined(_HOST_BUILD) */
///* #if defined(TISP_DEBUGPRINT) */
///*    mma::set_debug_level(TISP_DEBUGPRINT); */
///*    streaming_engine::set_debug_level(TISP_DEBUGPRINT); */
///*    sa_generator::set_debug_level(TISP_DEBUGPRINT); */
///* #endif */
///* #endif */
///* #endif */
///* } */
///* #ifdef __cplusplus */
///* } */
///* #endif */
