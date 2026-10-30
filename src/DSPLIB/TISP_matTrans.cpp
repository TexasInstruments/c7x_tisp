// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_DSPLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_matTrans.hpp>
#include <cstring>

namespace TISP {
namespace DSPLIB {

//! [DSPLIB_MatTrans constructor]

template <typename T>
MatTrans<T>::MatTrans(const TISP_BufDescList *pInputs,
                      const TISP_BufDescList *pOutputs,
                      const char             *nodeName,
                      uint32_t                nodeId,
                      ErrorCtxt              &errorCtx)
    : _in{}, _out{}
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;

   // Buffer checks: one input, one output
   if (TISP_checkBufDescList<T>(pInputs, 1U, 1U, "MatTrans constructor (inputs)", errorCtx) &&
       TISP_checkBufDescList<T>(pOutputs, 1U, 1U, "MatTrans constructor (outputs)", errorCtx)) {
      _in  = pInputs->pDesc[0];
      _out = pOutputs->pDesc[0];

      // DSPLIB_matTrans_init_checkParams does not compare the output dimensions with the input
      if ((_out.width != _in.height) || (_out.height != _in.width)) {
         errorCtx.setInvalidParameterError("MatTrans constructor",
                                           "output width and height must be the input height and width");
      }
   }

   if (errorCtx.isSuccess()) {
      // Initialize _params and bufParams with the node parameters
      (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", _in.width, _in.height);
      TISP_setNodeBufParams(&_nodeInfo.bufParams, pInputs, pOutputs);

      /***********************************************************************/
      /* Initialize data structures required by DSPLIB_matrTrans from DSPLIB */
      /***********************************************************************/

      DSPLIB_matTransInitArgs kerInitArgs;
      kerInitArgs.dimX      = _in.width;
      kerInitArgs.dimY      = _in.height;
      kerInitArgs.funcStyle = DSPLIB_FUNCTION_OPTIMIZED;

      _handle = make_unique_handle<char>(DSPLIB_matTrans_getHandleSize(&kerInitArgs));

      if (!_handle) {
         errorCtx.setMemoryError("MatTrans constructor", "failed to allocate matTrans handle");
      }
      else {
         DSPLIB_bufParams2D_t bufParamsIn  = TISP_toDsplibBufParams2D<T>(_in);
         DSPLIB_bufParams2D_t bufParamsOut = TISP_toDsplibBufParams2D<T>(_out);

         DSPLIB_STATUS status =
             DSPLIB_matTrans_init_checkParams(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);

         /******************************************************************************/
         /* Call init function from DSPLIB to for matrix transpose to set SE/SA params */
         /******************************************************************************/

         if (errorCtx.remapXLIBError(status, "DSPLIB_matTrans_init_checkParams")) {
            status = DSPLIB_matTrans_init(_handle.get(), &bufParamsIn, &bufParamsOut, &kerInitArgs);
            errorCtx.remapXLIBError(status, "DSPLIB_matTrans_init");
         }
      }
   }
}

//! [DSPLIB_MatTrans constructor]

template <typename T> void *MatTrans<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? _in.pBuf : nullptr;
}

template <typename T> void *MatTrans<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? _out.pBuf : nullptr;
}

template <typename T> size_t MatTrans<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t MatTrans<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void MatTrans<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _in.pBuf = addr.src[0];
   }
   if (mode == kAddrModeOut) {
      _out.pBuf = addr.dst[0];
   }

   if (mode == kAddrModeInOut) {
      _in.pBuf  = addr.src[0];
      _out.pBuf = addr.dst[0];
   }
}

template <typename T> void MatTrans<T>::exec() noexcept { DSPLIB_matTrans_exec(_handle.get(), _in.pBuf, _out.pBuf); }

template class MatTrans<float>;
template class MatTrans<double>;

} // namespace DSPLIB
} // namespace TISP
