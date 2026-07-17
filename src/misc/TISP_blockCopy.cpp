// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_common.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_blockCopy.hpp>
#include <cstring>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

namespace TISP {
namespace misc {

template <typename T>
BlockCopy<T>::BlockCopy(T *pX, T *pY, size_t width, size_t height, ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY), _width(width), _height(height)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", width, height);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("BlockCopy constructor", "input pointer pIn0 is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("BlockCopy constructor", "output pointer pOut is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("BlockCopy constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("BlockCopy constructor", "height must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {
      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = width * sizeof(T);

      _nodeInfo.bufParams.outputDataType     = ElementDType<T>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = width * sizeof(T);

      _se0Param.ELETYPE = c7x::se_eletype<vec>::value;
      _se0Param.VECLEN  = c7x::se_veclen<vec>::value;
      _se0Param.DIMFMT  = __SE_DIMFMT_1D;

      _se0Param.ICNT0  = static_cast<uint32_t>(_width * _height);
      _sa0Param.VECLEN = c7x::sa_veclen<vec>::value;
      _sa0Param.DIMFMT = __SA_DIMFMT_1D;

      _sa0Param.ICNT0 = static_cast<uint32_t>(_width * _height);
   }
}

template <typename T>
BlockCopy<T>::BlockCopy(T          *pX,
                        T          *pY,
                        size_t      width,
                        size_t      height,
                        const char *nodeName,
                        uint32_t    nodeId,
                        ErrorCtxt  &errorCtx)
    : BlockCopy(pX, pY, width, height, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void BlockCopy<T>::exec_ci(T *RESTRICT pX, T *RESTRICT pY, size_t width, size_t height) noexcept
{

   // open SE0 to fetch Input samples
   __SE0_OPEN(pX, _se0Param);

   // open SA0 to write output samples
   __SA0_OPEN(_sa0Param);

   for (size_t i = 0; i < (width * height) / (c7x::max_simd<T>::value); i++) {
      __vpred tmp  = c7x::strm_agen<0, vec>::get_vpred();
      vec    *addr = c7x::strm_agen<0, vec>::get_adv(pY);
      __vstore_pred(tmp, addr, c7x::strm_eng<0, vec>::get_adv());
   }
   // close SE/SA
   __SE0_CLOSE();
   __SA0_CLOSE();
}

template <typename T> void *BlockCopy<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> void *BlockCopy<T>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename T> size_t BlockCopy<T>::getNumInputs() const noexcept { return 1; }

template <typename T> size_t BlockCopy<T>::getNumOutputs() const noexcept { return 1; }

template <typename T> void BlockCopy<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept

{
   if (mode == kAddrModeIn) {
      _pX = static_cast<T *>(addr.src[0]);
   }
   if (mode == kAddrModeOut) {
      _pY = static_cast<T *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<T *>(addr.src[0]);
      _pY = static_cast<T *>(addr.dst[0]);
   }
}

template <typename T> void BlockCopy<T>::exec() noexcept { exec_ci(_pX, _pY, _width, _height); }

template class BlockCopy<float>;

} // namespace misc
} // namespace TISP
