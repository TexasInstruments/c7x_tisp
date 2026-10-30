// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_types.hpp"
#include "TISP_virtualOp.hpp"
#include <cstddef>

namespace TISP {

/**
 * @brief   Validates a group of buffer descriptors for a node of element type @c T.
 *
 * @details A null @c pList is treated as an empty group, which is an error only when @c minBufs is
 *          non-zero. Otherwise checks, in order: the buffer count lies in [@c minBufs, @c maxBufs]; the descriptor
 *          array is non-null when the group is non-empty; and, for every descriptor, a non-null
 *          @c pBuf, a @c dataType matching @c T, non-zero @c width and @c height, a @c pitchBytes
 *          of at least one row, and an @c isInterleave of `0` or `1`. The first failure is
 *          reported through @c errorCtx and the remaining checks are skipped.
 *
 * @tparam T : Node element type.
 *
 * @param [in]     pList    : Descriptor group to validate, or `nullptr` for an empty group.
 * @param [in]     minBufs  : Minimum number of buffers the node accepts in this group.
 * @param [in]     maxBufs  : Maximum number of buffers the node accepts in this group.
 * @param [in]     funcName : Caller identification used in the error message.
 * @param [in,out] errorCtx : Error context for reporting validation errors.
 *
 * @retval true  : Every descriptor is valid (and @c errorCtx was already successful).
 * @retval false : Validation failed, or @c errorCtx already held an error.
 * @throw None
 */
template <typename T>
bool TISP_checkBufDescList(const TISP_BufDescList *pList,
                           size_t                  minBufs,
                           size_t                  maxBufs,
                           const char             *funcName,
                           ErrorCtxt              &errorCtx) noexcept
{
   const size_t numBufs = (pList != nullptr) ? pList->numBufs : 0U;

   if (!errorCtx.isSuccess()) {
      /* Nothing to do here: an earlier check already failed */
   }
   else if ((pList == nullptr) && (minBufs > 0U)) {
      errorCtx.setNullPointerError(funcName, "buffer descriptor list is null");
   }
   else if ((numBufs < minBufs) || (numBufs > maxBufs)) {
      errorCtx.setInvalidParameterError(funcName, "unexpected number of buffers");
   }
   else if ((numBufs > 0U) && (pList->pDesc == nullptr)) {
      errorCtx.setNullPointerError(funcName, "buffer descriptor array is null");
   }
   else {
      for (size_t i = 0U; (i < numBufs) && errorCtx.isSuccess(); i++) {
         const TISP_BufDesc2D &desc = pList->pDesc[i]; // NOLINT(MISRA.PTR.ARITH)
         if (desc.pBuf == nullptr) {
            errorCtx.setNullPointerError(funcName, "buffer pointer pBuf is null");
         }
         else if (desc.dataType != ElementDType<T>::value) {
            errorCtx.setInvalidParameterError(funcName, "buffer dataType does not match the node element type");
         }
         else if ((desc.width == 0U) || (desc.height == 0U)) {
            errorCtx.setInvalidParameterError(funcName, "buffer width and height must be greater than zero");
         }
         else if (desc.pitchBytes < (desc.width * sizeof(T))) {
            errorCtx.setInvalidParameterError(funcName, "buffer pitchBytes is smaller than one row");
         }
         else if (desc.isInterleave > 1U) {
            errorCtx.setInvalidParameterError(funcName, "buffer isInterleave must be either zero or one");
         }
         else {
            /* Nothing to do here */
         }
      }
   }

   return errorCtx.isSuccess();
}

/**
 * @brief   Copies input and output descriptor geometry into a node's @ref NodeBufParams.
 *
 * @details Per-buffer width, height, and pitch are copied for every descriptor. @ref NodeBufParams
 *          stores one data type and one interleave flag per direction, so those are taken from
 *          buffer 0 of each group. A node without inputs (or outputs) passes `nullptr` for that group.
 *
 * @param [out] pBufParams : Node buffer params to populate.
 * @param [in]  pInputs    : Input descriptor group validated with @ref TISP_checkBufDescList
 *                           (1 .. @ref kMaxSrcAddr buffers), or `nullptr` when the node has no inputs.
 * @param [in]  pOutputs   : Output descriptor group validated with @ref TISP_checkBufDescList
 *                           (1 .. @ref kMaxDstAddr buffers), or `nullptr` when the node has no outputs.
 *
 * @retval None
 * @throw None
 */
inline void TISP_setNodeBufParams(NodeBufParams          *pBufParams,
                                  const TISP_BufDescList *pInputs,
                                  const TISP_BufDescList *pOutputs) noexcept
{
   if (pInputs != nullptr) {
      for (size_t i = 0U; i < pInputs->numBufs; i++) {
         pBufParams->widthIn[i]      = pInputs->pDesc[i].width;      // NOLINT(MISRA.PTR.ARITH)
         pBufParams->heightIn[i]     = pInputs->pDesc[i].height;     // NOLINT(MISRA.PTR.ARITH)
         pBufParams->inPitchBytes[i] = pInputs->pDesc[i].pitchBytes; // NOLINT(MISRA.PTR.ARITH)
      }
      pBufParams->inputDataType     = pInputs->pDesc[0].dataType;
      pBufParams->isInputInterleave = pInputs->pDesc[0].isInterleave;
   }
   if (pOutputs != nullptr) {
      for (size_t j = 0U; j < pOutputs->numBufs; j++) {
         pBufParams->widthOut[j]      = pOutputs->pDesc[j].width;      // NOLINT(MISRA.PTR.ARITH)
         pBufParams->heightOut[j]     = pOutputs->pDesc[j].height;     // NOLINT(MISRA.PTR.ARITH)
         pBufParams->outPitchBytes[j] = pOutputs->pDesc[j].pitchBytes; // NOLINT(MISRA.PTR.ARITH)
      }
      pBufParams->outputDataType     = pOutputs->pDesc[0].dataType;
      pBufParams->isOutputInterleave = pOutputs->pDesc[0].isInterleave;
   }
}

} // namespace TISP
