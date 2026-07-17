// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#ifndef COMMON_TI_MEMORY_H_
#define COMMON_TI_MEMORY_H_ 1

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "TI_profile.h"

/* Handle COFF/ELF for linker cmd file symbols */
#if defined(__TI_EABI__)
#define kernel_size _kernel_size
#define data_size _data_size
#endif

#ifdef _HOST_BUILD
#ifdef _MSC_VER /* If using MS Visual Studio Compiler */
#define align_free _aligned_free
#else
#define align_free (free)
#endif
#else /* _HOST_BUILD */
#define align_free (free)
#endif /* _HOST_BUILD */

#if SOC == j721e
#define TISP_L3_RESULTS_BUFFER_SIZE 4 * 1024 * 1024

#elif SOC == j721s2
#define TISP_L3_RESULTS_BUFFER_SIZE 3 * 1024 * 1024

#elif SOC == j784s4
#define TISP_L3_RESULTS_BUFFER_SIZE 2 * 1024 * 1024

#elif SOC == AM62A
#define TISP_L3_RESULTS_BUFFER_SIZE 800 * 1024 // 800 Kbytes to start with out of 1 MB in EL2, sect2
#define TISP_L2_BUFFER_SIZE 256 * 1024         // 800 Kbytes to start with out of 1 MB in EL2, sect2

#elif SOC == AM62D
#define TISP_L3_RESULTS_BUFFER_SIZE 800 * 1024 // 800 Kbytes to start with out of 1 MB in EL2, sect2
#define TISP_L2_BUFFER_SIZE 256 * 1024         // 800 Kbytes to start with out of 1 MB in EL2, sect2

#elif SOC == j722s
#define TISP_L3_RESULTS_BUFFER_SIZE 800 * 1024 // 800 Kbytes to start with out of 1 MB in EL2, sect2
#define TISP_L2_BUFFER_SIZE 256 * 1024         // 800 Kbytes to start with out of 1 MB in EL2, sect2

#elif SOC == AM275
#define TISP_L3_RESULTS_BUFFER_SIZE 800 * 1024 // 800 Kbytes to start with out of 1 MB in EL2, sect2
#define TISP_L2_BUFFER_SIZE 256 * 1024         // 800 Kbytes to start with out of 1 MB in EL2, sect2

#elif SOC == AM67A
#define TISP_L3_RESULTS_BUFFER_SIZE 800 * 1024 // 800 Kbytes to start with out of 1 MB in EL2, sect2
#else
#error invalid SOC
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Defines for types of test sequence data */
#define CONSTANT 0
#define SEQUENTIAL 1
#define SEQUENTIAL_NEGATIVE 2
#define RANDOM 3
#define RANDOM_SIGNED 4
#define STATIC 5
#define CUSTOM 6
#define RANDOM_DIMENSION 7
#define RANDOM_FAST 8
#define RANDOM_SMALL 9
#define SYMMETRIC 10

/* Used in linker CMD file to determine code/data size */
extern uint32_t kernel_size;
extern uint32_t data_size;

/* 2D Memory handling utilities in TI_memory.c */
int32_t TI_compare_mem(void *a, void *b, int32_t n);
int32_t TI_compare_mem_2D(void    *a,
                          void    *b,
                          int64_t  tolerance,
                          int32_t  wrap_exception,
                          uint32_t width,
                          uint32_t height,
                          uint32_t stride,
                          uint8_t  elementSize);
int32_t TI_compare_mem_2DDualStride(void    *a,
                                    void    *b,
                                    int64_t  tolerance,
                                    int32_t  wrap_exception,
                                    uint32_t width,
                                    uint32_t height,
                                    uint32_t strideA,
                                    uint32_t strideB,
                                    uint8_t  elementSize);
int32_t TI_compare_mem_roi2D(void    *a,
                             void    *b,
                             int32_t  tolerance,
                             int32_t  wrap_exception,
                             uint32_t width,
                             uint32_t height,
                             uint32_t stride,
                             uint8_t  elementSize);
int32_t TI_compare_mem_roi2DStatic(void    *a,
                                   void    *b,
                                   int32_t  tolerance,
                                   int32_t  wrap_exception,
                                   uint32_t width,
                                   uint32_t height,
                                   uint32_t stride,
                                   uint8_t  elementSize);

int32_t TI_compare_mem_2D_float(void    *a,
                                void    *b,
                                double   log10tolerance,
                                double   abstolerance,
                                uint32_t width,
                                uint32_t height,
                                uint32_t stride,
                                uint8_t  elementSize);

int32_t TI_compare_mem_roi2DStatic_float(void    *a,
                                         void    *b,
                                         double   log10tolerance,
                                         double   abstolerance,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t stride,
                                         uint8_t  elementSize);

void fillConst(uint32_t val, void *array, uint32_t width, uint32_t height, uint32_t stride, uint8_t elementSize);
void fillSeq(void *array, uint32_t width, uint32_t height, uint32_t stride, uint8_t elementSize);
void fillSeqNeg(void *array, uint32_t width, uint32_t height, uint32_t stride, uint8_t elementSize);
void fillRand(void *array, uint32_t width, uint32_t height, uint32_t stride, uint8_t elementSize);
void fillRandSigned(void *array, uint32_t width, uint32_t height, uint32_t stride, uint8_t elementSize);
// void fillRandFast(void *array, uint32_t width, uint32_t height, uint32_t stride, uint8_t elementSize);
void copyRoi(void *arrayDst, void *arraySrc, uint32_t width, uint32_t height, uint32_t stride, uint8_t elementSize);
void copyRoi_float_circular(void    *arrayDst,
                            void    *arraySrc,
                            uint32_t width,
                            uint32_t height,
                            uint32_t stride,
                            uint32_t bufferSize,
                            uint32_t startOffset,
                            uint8_t  elementSize);
void copyRoiDualStride_float(void    *arrayDst,
                             void    *arraySrc,
                             uint32_t width,
                             uint32_t height,
                             uint32_t strideDst,
                             uint32_t strideSrc,
                             uint8_t  elementSize);
void copyRoi_float3D(void    *arrayDst,
                     void    *arraySrc,
                     uint32_t width,
                     uint32_t height,
                     uint32_t length,
                     uint32_t stride_y,
                     uint32_t stride_z,
                     uint8_t  elementSize);
void copyRoi3D(void    *arrayDst,
               void    *arraySrc,
               uint32_t dim_x,
               uint32_t dim_y,
               uint32_t dim_z,
               uint32_t stride_y,
               uint32_t stride_z,
               uint8_t  elementSize);
void copyRoiDualStride(void    *arrayDst,
                       void    *arraySrc,
                       uint32_t width,
                       uint32_t height,
                       uint32_t strideDst,
                       uint32_t strideSrc,
                       uint8_t  elementSize);
void TI_fillBuffer(uint8_t  testPattern,
                   uint8_t  constFill,
                   void    *dest,
                   void    *src,
                   uint32_t width,
                   uint32_t height,
                   uint32_t stride,
                   uint8_t  elementSize,
                   char    *testPatternType);

void     TI_fillBuffer3D_float(uint8_t  testPattern,
                               uint8_t  constFill,
                               void    *dest,
                               void    *src,
                               uint32_t dim_x,
                               uint32_t dim_y,
                               uint32_t dim_z,
                               uint32_t stride_y,
                               uint32_t stride_z,
                               uint8_t  elementSize,
                               char    *testPatternType);
uint32_t compare(uint8_t *dsp_output, uint8_t *natc_output, uint32_t width, uint32_t height, uint32_t stride);
uint32_t TI_randomDimensionLessThan(uint32_t maxValue);

/* 3D Memory handling utilities in TI_memory.c */
int32_t TI_compare_mem_3D(void    *a,
                          void    *b,
                          int32_t  tolerance,
                          int32_t  wrap_exception,
                          uint32_t dim_x,
                          uint32_t dim_y,
                          uint32_t dim_z,
                          uint32_t stride_y,
                          uint32_t stride_z,
                          uint8_t  elementSize);
// int32_t TI_compare_mem_roi3D(void *a, void *b, int32_t tolerance, int32_t wrap_exception, uint32_t dim_x, uint32_t
// dim_y, uint32_t dim_z, uint32_t stride_y, uint32_t stride_z, uint8_t elementSize);
int32_t TI_compare_mem_roi3DStatic(void    *a,
                                   void    *b,
                                   int32_t  tolerance,
                                   int32_t  wrap_exception,
                                   uint32_t dim_x,
                                   uint32_t dim_y,
                                   uint32_t dim_z,
                                   uint32_t stride_y,
                                   uint32_t stride_z,
                                   uint8_t  elementSize);
int32_t TI_compare_mem_3D_float(void    *a,
                                void    *b,
                                double   log10tolerance,
                                double   abstolerance,
                                uint32_t dim_x,
                                uint32_t dim_y,
                                uint32_t dim_z,
                                uint32_t stride_y,
                                uint32_t stride_z,
                                uint8_t  elementSize);
void    fillConst3D(uint32_t val,
                    void    *array,
                    uint32_t dim_x,
                    uint32_t dim_y,
                    uint32_t dim_z,
                    uint32_t stride_y,
                    uint32_t stride_z,
                    uint8_t  elementSize);
void    fillSeq3D(void    *array,
                  uint32_t dim_x,
                  uint32_t dim_y,
                  uint32_t dim_z,
                  uint32_t stride_y,
                  uint32_t stride_z,
                  uint8_t  elementSize);
void    fillSeqNeg3D(void    *array,
                     uint32_t dim_x,
                     uint32_t dim_y,
                     uint32_t dim_z,
                     uint32_t stride_y,
                     uint32_t stride_z,
                     uint8_t  elementSize);
void    fillRand3D(void    *array,
                   uint32_t dim_x,
                   uint32_t dim_y,
                   uint32_t dim_z,
                   uint32_t stride_y,
                   uint32_t stride_z,
                   uint8_t  elementSize);
void    copyRoi3D(void    *arrayDst,
                  void    *arraySrc,
                  uint32_t dim_x,
                  uint32_t dim_y,
                  uint32_t dim_z,
                  uint32_t stride_y,
                  uint32_t stride_z,
                  uint8_t  elementSize);
void    TI_fillBuffer3D(uint8_t  testPattern,
                        uint8_t  constFill,
                        void    *dest,
                        void    *src,
                        uint32_t dim_x,
                        uint32_t dim_y,
                        uint32_t dim_z,
                        uint32_t stride_y,
                        uint32_t stride_z,
                        uint8_t  elementSize,
                        char    *testPatternType);
int32_t TI_compare_mem_3DDualStride(void    *a,
                                    void    *b,
                                    int32_t  tolerance,
                                    int32_t  wrap_exception,
                                    uint32_t dim_x,
                                    uint32_t dim_y,
                                    uint32_t dim_z,
                                    uint32_t strideA_y,
                                    uint32_t strideA_z,
                                    uint32_t strideB_y,
                                    uint32_t strideB_z,
                                    uint8_t  elementSize);

int32_t TI_compare_mem_2DDualStride_float(void    *a,
                                          void    *b,
                                          double   log10tolerance,
                                          double   abstolerance,
                                          uint32_t width,
                                          uint32_t height,
                                          uint32_t strideA,
                                          uint32_t strideB,
                                          uint8_t  elementSize);

/* 2D Float Memory handling utilities in TI_memory.c */
void copyRoi_float(void    *arrayDst,
                   void    *arraySrc,
                   uint32_t width,
                   uint32_t height,
                   uint32_t stride,
                   uint8_t  elementSize);
void fillRand_float(void *array, uint32_t width, uint32_t height, uint32_t stride, uint8_t elementSize);
void TI_fillBuffer_float(uint8_t  testPattern,
                         void    *constFill,
                         void    *dest,
                         void    *src,
                         uint32_t width,
                         uint32_t height,
                         uint32_t stride,
                         uint8_t  elementSize,
                         char    *testPatternType);

/* Display memory reqirements */
static inline void TI_kernel_memory(void)
{
#if !defined(__ONESHOTTEST) && !defined(_HOST_BUILD)
//    uint32_t kern_size = (uint32_t)((uint64_t)&kernel_size & 0xFFFFFFFF);
//    uint32_t dat_size = (uint32_t)((uint64_t)&data_size & 0xFFFFFFFF);
#endif /* __ONESHOTTEST */

   sprintf(TI_test_print_buffer,
           "-----------------------------------------------------------------------------------------------------------"
           "----------------------------------------------------------------------\n");
   TI_test_print_string(TI_test_print_buffer);
#if !defined(__ONESHOTTEST) && !defined(_HOST_BUILD)
   /* Provide memory information */
   sprintf(TI_test_print_buffer, "Memory Usage:\n");
   TI_test_print_string(TI_test_print_buffer);
   TI_test_print_string(TI_test_print_buffer);
   TI_test_print_string(TI_test_print_buffer);
#endif /* __ONESHOTTEST */
   TI_profile_stack_memory();
}

/* Run-time initialation of the fast heap for library
 * Called only if heap is linked into L2SRAM */
uint32_t TI_meminit(uint32_t size);

/* Following should be called to allocate buffers passed to kernel functions
 * for each test vector */
void *TI_memalign(size_t alignment, size_t size);
void *TI_malloc(size_t size);

/* Following should be called to de-allocate buffers passed to kernel functions
 * for each test vector */
void    TI_align_free(void *ptr);
void    TI_free(void *ptr);
int32_t TI_get_heap_size(void);
void    TI_memError(char *fn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* COMMON_TI_MEMORY_H_ */
