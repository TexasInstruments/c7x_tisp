// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

/********************************************************************************
 *  @file     DSPLIB_cascadeBiquad_priv.h
 *
 *  @brief    File to hold private parameter of function DSPLIB_cascadeBiquad
 *
 ********************************************************************************/

#ifndef DSPLIB_CASCADEBIQUAD_PRIV_H_
#define DSPLIB_CASCADEBIQUAD_PRIV_H_

// #include "../common/DSPLIB_utility.h"
#include "dsplib.h"

/**
 * \file
 * \brief Header file for kernel's internal use. For the kernel's interface, please
 *        see @ref DSPLIB_cascadeBiquad
 */

/*******************************************************************************
 *
 * DEFINES
 *
 ******************************************************************************/

/**
 * @brief Macro to define the number of coefficients per TI biquad.
 *
 * @details
 *      This macro defines the number of coefficients per TI biquad.
 *
 * @note
 *      This macro is used in the cascade biquad kernel.
 *
 * @sa
 *      DSPLIB_cascadeBiquad
 */
#define DSPLIB_NUM_TIF_BIQUAD_COEFS (4)

/**
 * @brief Macro to define the number of gains per TI biquad stage group.
 *
 * @details
 *      This macro defines the number of gains per TI biquad stage group.
 *
 * @note
 *      This macro is used in the cascade biquad kernel.
 *
 * @sa
 *      DSPLIB_cascadeBiquad
 */
#define DSPLIB_NUM_TIF_BIQUAD_GAINS (1)

/**
 * @brief Macro to define the low stage count threshold.
 *
 * @details
 *      This macro defines the low stage count threshold.
 *      The low stage count cascade biquad implementation is
 *      executed when the stage count at or below this threshold.
 *
 * @note
 *      This macro is used in the cascade biquad kernel.
 *
 * @sa
 *      DSPLIB_cascadeBiquad
 */
#define DSPLIB_CASCADEBIQUAD_TIF_THR_LOW_STAGE_COUNT (3)

#define DSPLIB_CASCADEBIQUAD_NUM_STAGES_1 (1) ///< Define 1 stage in cascade biquad
#define DSPLIB_CASCADEBIQUAD_NUM_STAGES_2 (2) ///< Define 2 stages in cascade biquad
#define DSPLIB_CASCADEBIQUAD_NUM_STAGES_3 (3) ///< Define 3 stages in cascade biquad
#define DSPLIB_CASCADEBIQUAD_NUM_STAGES_4 (4) ///< Define 4 stages in cascade biquad
#define DSPLIB_CASCADEBIQUAD_NUM_STAGES_5 (5) ///< Define 5 stages in cascade biquad
#define DSPLIB_CASCADEBIQUAD_NUM_STAGES_6 (6) ///< Define 6 stages in cascade biquad
#define DSPLIB_CASCADEBIQUAD_NUM_STAGES_7 (7) ///< Define 7 stages in cascade biquad
#define DSPLIB_CASCADEBIQUAD_NUM_STAGES_8 (8) ///< Define 8 stages in cascade biquad

/**
 * @brief Macro to define the number of TI biquad stages per stage group.
 *
 * @details
 *      This macro defines the number of TI biquad stages per stage group.
 *      Stage groups are used when the number of stages in the cascade biquad is
 *      greater than 8.
 *
 * @note
 *      This macro is used in the cascade biquad kernel.
 *
 * @sa
 *      DSPLIB_cascadeBiquad
 */
#define DSPLIB_CASCADEBIQUAD_TIF_STAGES_PER_STAGE_GROUP (DSPLIB_CASCADEBIQUAD_NUM_STAGES_7)

/**
 * @brief Macro to define the number of TI biquad coefficients read per SE in 7-stage mode.
 *
 * @details
 *      This macro defines the number of TI biquad coefficients read per SE in 7-stage mode.
 *
 * @note
 *      This macro is used in the cascade biquad kernel.
 *
 * @sa
 *      DSPLIB_cascadeBiquad
 */
#define DSPLIB_CASCADEBIQUAD_TIF_SE_NUM_BIQUAD_COEF (DSPLIB_NUM_TIF_BIQUAD_COEFS / 2)

/**
 * @brief Macro to define the number of DF1 biquad stages per stage group.
 *
 * @details
 *      This macro defines the number of biquad stages per stage group in DF1 mode.
 *
 * @note
 *      This macro is used in the cascade biquad kernel.
 *
 * @sa
 *      DSPLIB_cascadeBiquad
 */
#define DSPLIB_CASCADEBIQUAD_DF1_STAGES_PER_STAGE_GROUP (DSPLIB_CASCADEBIQUAD_NUM_STAGES_2)

/**
 * @brief Macro to define the number of DF2 biquad stages per stage group.
 *
 * @details
 *      This macro defines the number of biquad stages per stage group in DF2 mode.
 *
 * @note
 *      This macro is used in the cascade biquad kernel.
 *
 * @sa
 *      DSPLIB_cascadeBiquad
 */
#define DSPLIB_CASCADEBIQUAD_DF2_STAGES_PER_STAGE_GROUP (DSPLIB_CASCADEBIQUAD_NUM_STAGES_2)

/**
 * @brief Macro to define the number of TDF2 biquad stages per stage group.
 *
 * @details
 *      This macro defines the number of biquad stages per stage group in TDF2 mode.
 *
 * @note
 *      This macro is used in the cascade biquad kernel.
 *
 * @sa
 *      DSPLIB_cascadeBiquad
 */
#define DSPLIB_CASCADEBIQUAD_TDF2_STAGES_PER_STAGE_GROUP (DSPLIB_CASCADEBIQUAD_NUM_STAGES_3)

/**
 * @brief Macro to define the size of bufPblock array of
 *        @ref DSPLIB_cascadeBiquad_PrivArgs structure.
 *
 */
#define DSPLIB_CASCADEBIQUAD_PBLOCK_SIZE (6 * SE_PARAM_SIZE + 1 * DSPLIB_PARAM_SIZE + 2 * sizeof(int32_t))

/**
 * @name SE/SA template offsets in parameter block
 *
 * @details
 *      These macros define the offsets of the SE and SA templates in the
 *      parameter block.
 *
 * @{
 */
#define SE_PARAM_BASE (0x0000)                                    /**< Base offset of parameter block. */
#define SE_SE0_PARAM_OFFSET (SE_PARAM_BASE)                       /**< Offset of SE0 template. */
#define SE_SE1_PARAM_OFFSET (SE_SE0_PARAM_OFFSET + SE_PARAM_SIZE) /**< Offset of SE1 template. */
#define SE_SA0_PARAM_OFFSET (SE_SE1_PARAM_OFFSET + SE_PARAM_SIZE) /**< Offset of SA0 template. */
#define SE_SA1_PARAM_OFFSET (SE_SA0_PARAM_OFFSET + SE_PARAM_SIZE) /**< Offset of SA1 template. */
#define SE_SA2_PARAM_OFFSET (SE_SA1_PARAM_OFFSET + SE_PARAM_SIZE) /**< Offset of SA2 template. */
#define SE_SA3_PARAM_OFFSET (SE_SA2_PARAM_OFFSET + SE_PARAM_SIZE) /**< Offset of SA3 template. */
/** @} */

/**
 *  @brief This is a function pointer type that conforms to the
 *         declaration of @ref DSPLIB_cascadeBiquad_exec_ci
 *         and @ref DSPLIB_cascadeBiquad_exec_cn.
 */
typedef DSPLIB_STATUS (*pFxnDSPLIB_cascadeBiquad_exec)(DSPLIB_kernelHandle handle,
                                                       void *restrict pIn,
                                                       void *restrict pFilterCoeff,
                                                       void *restrict pFilterVar,
                                                       void *restrict pOut);

/**
 *  @brief This function is the initialization function for the C7x low-stage count
 *         implementation of the kernel with the same coefficients fo each channel.
 *         The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_init.
 *
 * @details This function determines and saves the configuration for the
 *          C7x streaming engine and streaming address generator hardware
 *          for the low-stage count implementations. The configuration is
 *          saved in bufPBlock array. This is later retrieved and used by
 *          @ref DSPLIB_cascadeBiquad_exec_ci to set up the hardware resources.
 *          This approach eliminates the need to recompute the configuration
 *          during the kernel call sequence.
 *
 *  @param [in]  handle       :  Active handle to the kernel
 *  @param [in]  bufParamsIn  :  Pointer to the structure containing dimensional
 *                               information of input buffer
 *  @param [in]  bufParamsFilterCoeff :  Pointer to the structure containing dimensional
 *                               information of filter coefficient buffer
 *  @param [in/out]  bufParamsFilterVar :  Pointer to the structure containing dimensional
 *                               information of filter variable buffer
 *  @param [out] bufParamsOut :  Pointer to the structure containing dimensional
 *                               information of ouput buffer
 *  @param [in]  pKerInitArgs :  Pointer to the structure holding init parameters
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_lStage_init_ci(DSPLIB_kernelHandle                  handle,
                                                             const DSPLIB_bufParams2D_t          *bufParamsIn,
                                                             const DSPLIB_bufParams2D_t          *bufParamsFilterCoeff,
                                                             const DSPLIB_bufParams2D_t          *bufParamsFilterVar,
                                                             const DSPLIB_bufParams2D_t          *bufParamsOut,
                                                             const DSPLIB_cascadeBiquad_InitArgs *pKerInitArgs);

/**
 *  @brief This function is the initialization function for the C7x high-stage count
 *         implementation of the kernel with the same coefficients for each channel.
 *         The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquadMStage_init.
 *
 * @details This function determines and saves the configuration for the
 *          C7x streaming engine and streaming address generator hardware
 *          for the high-stage count implementations. The configuration is
 *          saved in bufPBlock array. This is later retrieved and used by
 *          @ref DSPLIB_cascadeBiquad_exec_ci to set up the hardware resources.
 *          This approach eliminates the need to recompute the configuration
 *          during the kernel call sequence.

 *  @param [in]  handle       :  Active handle to the kernel
 *  @param [in]  bufParamsIn  :  Pointer to the structure containing dimensional
 *                               information of input buffer
 *  @param [in]  bufParamsFilterCoeff :  Pointer to the structure containing dimensional
 *                               information of filter coefficient buffer
 *  @param [in/out]  bufParamsFilterVar :  Pointer to the structure containing dimensional
 *                               information of filter variable buffer
 *  @param [out] bufParamsOut :  Pointer to the structure containing dimensional
 *                               information of ouput buffer
 *  @param [in]  pKerInitArgs :  Pointer to the structure holding init parameters
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_hStage_init_ci(DSPLIB_kernelHandle                  handle,
                                                             const DSPLIB_bufParams2D_t          *bufParamsIn,
                                                             const DSPLIB_bufParams2D_t          *bufParamsFilterCoeff,
                                                             const DSPLIB_bufParams2D_t          *bufParamsFilterVar,
                                                             const DSPLIB_bufParams2D_t          *bufParamsOut,
                                                             const DSPLIB_cascadeBiquad_InitArgs *pKerInitArgs);

/**
 *  @brief This function is the 1-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declaration conforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 1-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_1Stage_exec_ci(DSPLIB_kernelHandle handle,
                                                      void *restrict pIn,
                                                      void *restrict pFilterCoeff,
                                                      void *restrict pFilterVar,
                                                      void *restrict pOut);

/**
 *  @brief This function is the 2-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declaration conforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 2-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_2Stage_exec_ci(DSPLIB_kernelHandle handle,
                                                      void *restrict pIn,
                                                      void *restrict pFilterCoeff,
                                                      void *restrict pFilterVar,
                                                      void *restrict pOut);

/**
 *  @brief This function is the 3-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declaration conforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 3-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_3Stage_exec_ci(DSPLIB_kernelHandle handle,
                                                             void *restrict pIn,
                                                             void *restrict pFilterCoeff,
                                                             void *restrict pFilterVar,
                                                             void *restrict pOut);

/**
 *  @brief This function is the 4-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declaration conforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 4-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_4Stage_exec_ci(DSPLIB_kernelHandle handle,
                                                             void *restrict pIn,
                                                             void *restrict pFilterCoeff,
                                                             void *restrict pFilterVar,
                                                             void *restrict pOut);

/**
 *  @brief This function is the 5-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declarationconforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 5-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_5Stage_exec_ci(DSPLIB_kernelHandle handle,
                                                             void *restrict pIn,
                                                             void *restrict pFilterCoeff,
                                                             void *restrict pFilterVar,
                                                             void *restrict pOut);

/**
 *  @brief This function is the 6-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declarationconforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 6-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_6Stage_exec_ci(DSPLIB_kernelHandle handle,
                                                             void *restrict pIn,
                                                             void *restrict pFilterCoeff,
                                                             void *restrict pFilterVar,
                                                             void *restrict pOut);

/**
 *  @brief This function is the 8-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declarationconforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 8-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_8Stage_exec_ci(DSPLIB_kernelHandle handle,
                                                             void *restrict pIn,
                                                             void *restrict pFilterCoeff,
                                                             void *restrict pFilterVar,
                                                             void *restrict pOut);

/**
 *  @brief This function is the M-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declarationconforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the M-stage cascade biquad, processing
 *          the cascade in 7-stage groups. Streaming engine and streaming address
 *          generators are configured using the information saved in bufPBlock
 *          array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_MStage_exec_ci(DSPLIB_kernelHandle handle,
                                                             void *restrict pIn,
                                                             void *restrict pFilterCoeff,
                                                             void *restrict pFilterVar,
                                                             void *restrict pOut);

/**
 *  @brief This function is the initialization function for the C7x low-stage count
 *         implementation of the kernel with different coefficients fo each channel.
 *         The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_init.
 *
 * @details This function determines and saves the configuration for the
 *          C7x streaming engine and streaming address generator hardware
 *          for the low-stage count implementations. The configuration is
 *          saved in bufPBlock array. This is later retrieved and used by
 *          @ref DSPLIB_cascadeBiquad_exec_ci to set up the hardware resources.
 *          This approach eliminates the need to recompute the configuration
 *          during the kernel call sequence.
 *
 *  @param [in]  handle       :  Active handle to the kernel
 *  @param [in]  bufParamsIn  :  Pointer to the structure containing dimensional
 *                               information of input buffer
 *  @param [in]  bufParamsFilterCoeff :  Pointer to the structure containing dimensional
 *                               information of filter coefficient buffer
 *  @param [in/out]  bufParamsFilterVar :  Pointer to the structure containing dimensional
 *                               information of filter variable buffer
 *  @param [out] bufParamsOut :  Pointer to the structure containing dimensional
 *                               information of ouput buffer
 *  @param [in]  pKerInitArgs :  Pointer to the structure holding init parameters
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_lStage_Nch_init_ci(DSPLIB_kernelHandle         handle,
                                                                 const DSPLIB_bufParams2D_t *bufParamsIn,
                                                                 const DSPLIB_bufParams2D_t *bufParamsFilterCoeff,
                                                                 const DSPLIB_bufParams2D_t *bufParamsFilterVar,
                                                                 const DSPLIB_bufParams2D_t *bufParamsOut,
                                                                 const DSPLIB_cascadeBiquad_InitArgs *pKerInitArgs);

/**
 *  @brief This function is the initialization function for the C7x high-stage count
 *         implementation of the kernel with different coefficients for each channel.
 *         The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquadMStage_init.
 *
 * @details This function determines and saves the configuration for the
 *          C7x streaming engine and streaming address generator hardware
 *          for the high-stage count implementations. The configuration is
 *          saved in bufPBlock array. This is later retrieved and used by
 *          @ref DSPLIB_cascadeBiquad_exec_ci to set up the hardware resources.
 *          This approach eliminates the need to recompute the configuration
 *          during the kernel call sequence.

 *  @param [in]  handle       :  Active handle to the kernel
 *  @param [in]  bufParamsIn  :  Pointer to the structure containing dimensional
 *                               information of input buffer
 *  @param [in]  bufParamsFilterCoeff :  Pointer to the structure containing dimensional
 *                               information of filter coefficient buffer
 *  @param [in/out]  bufParamsFilterVar :  Pointer to the structure containing dimensional
 *                               information of filter variable buffer
 *  @param [out] bufParamsOut :  Pointer to the structure containing dimensional
 *                               information of ouput buffer
 *  @param [in]  pKerInitArgs :  Pointer to the structure holding init parameters
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_hStage_Nch_init_ci(DSPLIB_kernelHandle         handle,
                                                                 const DSPLIB_bufParams2D_t *bufParamsIn,
                                                                 const DSPLIB_bufParams2D_t *bufParamsFilterCoeff,
                                                                 const DSPLIB_bufParams2D_t *bufParamsFilterVar,
                                                                 const DSPLIB_bufParams2D_t *bufParamsOut,
                                                                 const DSPLIB_cascadeBiquad_InitArgs *pKerInitArgs);

/**
 *  @brief This function is the 1-stage execution function for the C7x implementation
 *         of the kernel with different  coefficients for each channel. The function
 *         declaration conforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 1-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_1Stage_Nch_exec_ci(DSPLIB_kernelHandle handle,
                                                          void *restrict pIn,
                                                          void *restrict pFilterCoeff,
                                                          void *restrict pFilterVar,
                                                          void *restrict pOut);

/**
 *  @brief This function is the 2-stage execution function for the C7x implementation
 *         of the kernel with different coefficients for each channel. The function
 *         declaration conforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 2-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_2Stage_Nch_exec_ci(DSPLIB_kernelHandle handle,
                                                          void *restrict pIn,
                                                          void *restrict pFilterCoeff,
                                                          void *restrict pFilterVar,
                                                          void *restrict pOut);

/**
 *  @brief This function is the 3-stage execution function for the C7x implementation
 *         of the kernel with different coefficients for each channel. The function
 *         declaration conforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 3-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_3Stage_Nch_exec_ci(DSPLIB_kernelHandle handle,
                                                                 void *restrict pIn,
                                                                 void *restrict pFilterCoeff,
                                                                 void *restrict pFilterVar,
                                                                 void *restrict pOut);

/**
 *  @brief This function is the 4-stage execution function for the C7x implementation
 *         of the kernel with different coefficients for each channel. The function
 *         declaration conforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 4-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_4Stage_Nch_exec_ci(DSPLIB_kernelHandle handle,
                                                                 void *restrict pIn,
                                                                 void *restrict pFilterCoeff,
                                                                 void *restrict pFilterVar,
                                                                 void *restrict pOut);

/**
 *  @brief This function is the 5-stage execution function for the C7x implementation
 *         of the kernel with different coefficients for each channel. The function
 *         declarationconforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 5-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_5Stage_Nch_exec_ci(DSPLIB_kernelHandle handle,
                                                                 void *restrict pIn,
                                                                 void *restrict pFilterCoeff,
                                                                 void *restrict pFilterVar,
                                                                 void *restrict pOut);

/**
 *  @brief This function is the 6-stage execution function for the C7x implementation
 *         of the kernel with different coefficients for each channel. The function
 *         declarationconforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 6-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_6Stage_Nch_exec_ci(DSPLIB_kernelHandle handle,
                                                                 void *restrict pIn,
                                                                 void *restrict pFilterCoeff,
                                                                 void *restrict pFilterVar,
                                                                 void *restrict pOut);

/**
 *  @brief This function is the 8-stage execution function for the C7x implementation
 *         of the kernel with different coefficients for each channel. The function
 *         declarationconforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the 8-stage cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_8Stage_Nch_exec_ci(DSPLIB_kernelHandle handle,
                                                                 void *restrict pIn,
                                                                 void *restrict pFilterCoeff,
                                                                 void *restrict pFilterVar,
                                                                 void *restrict pOut);

/**
 *  @brief This function is the M-stage execution function for the C7x implementation
 *         of the kernel with the same coefficients for each channel. The function
 *         declarationconforms to the declaration of @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the M-stage cascade biquad, processing
 *          the cascade in 7-stage groups. Streaming engine and streaming address
 *          generators are configured using the information saved in bufPBlock
 *          array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tif_MStage_Nch_exec_ci(DSPLIB_kernelHandle handle,
                                                                 void *restrict pIn,
                                                                 void *restrict pFilterCoeff,
                                                                 void *restrict pFilterVar,
                                                                 void *restrict pOut);

/**
 *  @brief This function is the initialization function for the C7x implementation
 *         of the DF1 kernel. The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_init.
 *
 * @details This function determines and saves the configuration for the
 *          C7x streaming engine and streaming address generator hardware
 *          for the low-stage count implementations. The configuration is
 *          saved in bufPBlock array. This is later retrieved and used by
 *          @ref DSPLIB_cascadeBiquad_exec_ci to set up the hardware resources.
 *          This approach eliminates the need to recompute the configuration
 *          during the kernel call sequence.
 *
 *  @param [in]  handle       :  Active handle to the kernel
 *  @param [in]  bufParamsIn  :  Pointer to the structure containing dimensional
 *                               information of input buffer
 *  @param [in]  bufParamsFilterCoeff :  Pointer to the structure containing dimensional
 *                               information of filter coefficient buffer
 *  @param [in/out]  bufParamsFilterVar :  Pointer to the structure containing dimensional
 *                               information of filter variable buffer
 *  @param [out] bufParamsOut :  Pointer to the structure containing dimensional
 *                               information of ouput buffer
 *  @param [in]  pKerInitArgs :  Pointer to the structure holding init parameters
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_df1_init_ci(DSPLIB_kernelHandle                  handle,
                                                      const DSPLIB_bufParams2D_t          *bufParamsIn,
                                                      const DSPLIB_bufParams2D_t          *bufParamsFilterCoeff,
                                                      const DSPLIB_bufParams2D_t          *bufParamsFilterVar,
                                                      const DSPLIB_bufParams2D_t          *bufParamsOut,
                                                      const DSPLIB_cascadeBiquad_InitArgs *pKerInitArgs);

/**
 *  @brief This function is the execution function for the C7x implementation
 *         of the DF1 kernel. The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes DF1 cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_df1_exec_ci(DSPLIB_kernelHandle handle,
                                                      void *restrict pIn,
                                                      void *restrict pFilterCoeff,
                                                      void *restrict pFilterVar,
                                                      void *restrict pOut);

/**
 *  @brief This function is the initialization function for the C7x implementation
 *         of the DF2 kernel. The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_init.
 *
 * @details This function determines and saves the configuration for the
 *          C7x streaming engine and streaming address generator hardware
 *          for the low-stage count implementations. The configuration is
 *          saved in bufPBlock array. This is later retrieved and used by
 *          @ref DSPLIB_cascadeBiquad_exec_ci to set up the hardware resources.
 *          This approach eliminates the need to recompute the configuration
 *          during the kernel call sequence.
 *
 *  @param [in]  handle       :  Active handle to the kernel
 *  @param [in]  bufParamsIn  :  Pointer to the structure containing dimensional
 *                               information of input buffer
 *  @param [in]  bufParamsFilterCoeff :  Pointer to the structure containing dimensional
 *                               information of filter coefficient buffer
 *  @param [in/out]  bufParamsFilterVar :  Pointer to the structure containing dimensional
 *                               information of filter variable buffer
 *  @param [out] bufParamsOut :  Pointer to the structure containing dimensional
 *                               information of ouput buffer
 *  @param [in]  pKerInitArgs :  Pointer to the structure holding init parameters
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_df2_init_ci(DSPLIB_kernelHandle                  handle,
                                                      const DSPLIB_bufParams2D_t          *bufParamsIn,
                                                      const DSPLIB_bufParams2D_t          *bufParamsFilterCoeff,
                                                      const DSPLIB_bufParams2D_t          *bufParamsFilterVar,
                                                      const DSPLIB_bufParams2D_t          *bufParamsOut,
                                                      const DSPLIB_cascadeBiquad_InitArgs *pKerInitArgs);

/**
 *  @brief This function is the execution function for the C7x implementation
 *         of the DF2 kernel. The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the DF2 cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_df2_exec_ci(DSPLIB_kernelHandle handle,
                                                      void *restrict pIn,
                                                      void *restrict pFilterCoeff,
                                                      void *restrict pFilterVar,
                                                      void *restrict pOut);

/**
 *  @brief This function is the initialization function for the C7x implementation
 *         of the TDF2 kernel. The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_init.
 *
 * @details This function determines and saves the configuration for the
 *          C7x streaming engine and streaming address generator hardware
 *          for the low-stage count implementations. The configuration is
 *          saved in bufPBlock array. This is later retrieved and used by
 *          @ref DSPLIB_cascadeBiquad_exec_ci to set up the hardware resources.
 *          This approach eliminates the need to recompute the configuration
 *          during the kernel call sequence.
 *
 *  @param [in]  handle       :  Active handle to the kernel
 *  @param [in]  bufParamsIn  :  Pointer to the structure containing dimensional
 *                               information of input buffer
 *  @param [in]  bufParamsFilterCoeff :  Pointer to the structure containing dimensional
 *                               information of filter coefficient buffer
 *  @param [in/out]  bufParamsFilterVar :  Pointer to the structure containing dimensional
 *                               information of filter variable buffer
 *  @param [out] bufParamsOut :  Pointer to the structure containing dimensional
 *                               information of ouput buffer
 *  @param [in]  pKerInitArgs :  Pointer to the structure holding init parameters
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tdf2_init_ci(DSPLIB_kernelHandle                  handle,
                                                       const DSPLIB_bufParams2D_t          *bufParamsIn,
                                                       const DSPLIB_bufParams2D_t          *bufParamsFilterCoeff,
                                                       const DSPLIB_bufParams2D_t          *bufParamsFilterVar,
                                                       const DSPLIB_bufParams2D_t          *bufParamsOut,
                                                       const DSPLIB_cascadeBiquad_InitArgs *pKerInitArgs);

/**
 *  @brief This function is the execution function for the C7x implementation
 *         of the TDF2 kernel. The function declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details This function executes the TDF2 cascade biquad. Streaming
 *          engine and streaming address generators are configured using the
 *          information saved in bufPBlock array in @ref DSPLIB_cascadeBiquad_init_ci.
 *          Data is processed in multi-channel fashion.
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilter     : Pointer to buffer holding the filter data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 *  @par Performance Considerations:
 *    For best performance,
 *    - the data buffers are expected to be in L2 memory
 *    - the buffer pointers are assumed to be 64-byte aligned
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tdf2_exec_ci(DSPLIB_kernelHandle handle,
                                                       void *restrict pIn,
                                                       void *restrict pFilterCoeff,
                                                       void *restrict pFilterVar,
                                                       void *restrict pOut);

/**
 * @brief Structure that is reserved for internal use by the kernel
 */
typedef struct {
   /** @brief Structure holding initialization parameters                 */
   DSPLIB_cascadeBiquad_InitArgs initArgs;
   /** @brief Function pointer to point to the right execution variant
             @ref DSPLIB_cascadeBiquad_exec_cn and
    *         @ref DSPLIB_cascadeBiquad_exec_ci.                        */
   pFxnDSPLIB_cascadeBiquad_exec execute;
   /** @brief Pitch of input buffer for different batches
    *         @ref DSPLIB_cascadeBiquad_init that will be retrieved
    *         and used by @ref DSPLIB_cascadeBiquad_exec                */
   uint32_t dataBufferInPitch;
   /** @brief Pitch of output buffer for different batches
    *         @ref DSPLIB_cascadeBiquad_init that will be retrieved
    *         and used by @ref DSPLIB_cascadeBiquad_exec                */
   uint32_t dataBufferOutPitch;
   /** @brief Pitch of filter Variable buffer for different channels
    *         @ref DSPLIB_cascadeBiquad_init that will be retrieved
    *         and used by @ref DSPLIB_cascadeBiquad_exec                */
   uint32_t filterVarPitch;
   /** @brief Number of stage groups in the cascade biquad.
    *         Calculated by @ref DSPLIB_cascadeBiquad_init and
    *         used by @ref DSPLIB_cascadeBiquad_exec.                   */
   uint32_t numStageGroups;
   /** @brief Number of remaining stages in the last 2-stage group.
    *         Calculated by @ref DSPLIB_cascadeBiquad_init and
    *         used by @ref DSPLIB_cascadeBiquad_exec.                   */
   uint32_t stage2Cnt;
   /** @brief Number of remaining stages in the last 1-stage group.
    *         Calculated by @ref DSPLIB_cascadeBiquad_init and
    *         used by @ref DSPLIB_cascadeBiquad_exec.                   */
   uint32_t stage1Cnt;
   /** @brief Array to store SE/SA template configurations prepared
    *         by @ref DSPLIB_cascadeBiquad_init retrieved and used
    *         by @ref DSPLIB_cascadeBiquad_exec                         */
   uint8_t bufPblock[DSPLIB_CASCADEBIQUAD_PBLOCK_SIZE];
} DSPLIB_cascadeBiquad_PrivArgs;

/**
 *  @brief This function is the initialization function for the natural C
 *         implementation of the kernel. The function declaration conforms to
 *         the declaration of @ref DSPLIB_cascadeBiquad_init.
 *
 * @details
 *
 *  @param [in]  handle                 :  Active handle to the kernel
 *  @param [in]  bufParamsIn            :  Pointer to the structure containing dimensional
 *                                         information of input buffer
 *  @param [in]  bufParamsFilterCoeff   :  Pointer to the structure containing dimensional
 *                                         information of filter coefficient buffer
 *  @param [in/out]  bufParamsFilterVar :  Pointer to the structure containing dimensional
 *                                         information of filter variable buffer
 *  @param [out] bufParamsOut           :  Pointer to the structure containing dimensional
 *                                         information of ouput buffer
 *  @param [in]  pKerInitArgs           :  Pointer to the structure holding init parameters
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
DSPLIB_STATUS DSPLIB_cascadeBiquad_init_cn(DSPLIB_kernelHandle            handle,
                                           DSPLIB_bufParams2D_t          *bufParamsIn,
                                           DSPLIB_bufParams2D_t          *bufParamsFilterCoeff,
                                           DSPLIB_bufParams2D_t          *bufParamsFilterVar,
                                           DSPLIB_bufParams2D_t          *bufParamsOut,
                                           DSPLIB_cascadeBiquad_InitArgs *pKerInitArgs);

/**
 *  @brief This function is the main execution function for the natural C
 *         implementation of the DF2 biquad cascade kernel. The function
 *         declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilterCoeff: Pointer to buffer holding the filter coefficient data
 *  @param [in]  pFilterVar  : Pointer to buffer holding the filter variable data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_df2_exec_cn(DSPLIB_kernelHandle handle,
                                                      void *restrict pIn,
                                                      void *restrict pFilterCoeff,
                                                      void *restrict pFilterVar,
                                                      void *restrict pOut);

/**
 *  @brief This function is the main execution function for the natural C
 *         implementation of the DF1 biquad cascade kernel. The function
 *         declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilterCoeff: Pointer to buffer holding the filter coefficient data
 *  @param [in]  pFilterVar  : Pointer to buffer holding the filter variable data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_df1_exec_cn(DSPLIB_kernelHandle handle,
                                                      void *restrict pIn,
                                                      void *restrict pFilterCoeff,
                                                      void *restrict pFilterVar,
                                                      void *restrict pOut);

/**
 *  @brief This function is the main execution function for the natural C
 *         implementation of the TDF2 biquad cascade kernel. The function
 *         declaration conforms to the declaration of
 *         @ref DSPLIB_cascadeBiquad_exec.
 *
 * @details
 *
 *  @param [in]  handle      : Active handle to the kernel
 *  @param [in]  pIn         : Pointer to buffer holding the input data
 *  @param [in]  pFilterCoeff: Pointer to buffer holding the filter coefficient data
 *  @param [in]  pFilterVar  : Pointer to buffer holding the filter variable data
 *  @param [out] pOut        : Pointer to buffer holding the output data
 *
 *  @return      Status value indicating success or failure. Refer to @ref DSPLIB_STATUS.
 *
 */
template <typename dataType>
extern DSPLIB_STATUS DSPLIB_cascadeBiquad_tdf2_exec_cn(DSPLIB_kernelHandle handle,
                                                       void *restrict pIn,
                                                       void *restrict pFilterCoeff,
                                                       void *restrict pFilterVar,
                                                       void *restrict pOut);

#endif /* DSPLIB_CASCADEBIQUAD_PRIV_H_ */
