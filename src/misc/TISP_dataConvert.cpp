// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "../common/TISP_common.hpp"
#include "c7x_scalable.h"
#include <TISP_dataConvert.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(HOST_EMULATION)
#define RESTRICT
#else
#define RESTRICT restrict
#endif

namespace TISP {
namespace misc {

#define TISP_SCALE_INT32_TO_FLOAT (5.96046448e-8f)
#define TISP_SCALE_FLOAT_TO_INT32 (16777216.0f)

template <typename dTypeIn, typename dTypeOut>
DataConvert<dTypeIn, dTypeOut>::DataConvert(dTypeIn   *pX,
                                            dTypeOut  *pY,
                                            size_t     width,
                                            size_t     height,
                                            size_t     strideInBytes,
                                            size_t     strideOutBytes,
                                            ErrorCtxt &errorCtx)
    : _pX(pX), _pY(pY), _width(width), _height(height), _strideInBytes(strideInBytes), _strideOutBytes(strideOutBytes)
{

   // Initialize _params with the node parameters
   (void) snprintf(_nodeInfo.params.data(), _nodeInfo.params.size(), "width=%zu,height=%zu", width, height);

   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("DataConvert constructor", "input pointer pIn0 is null");
   }
   if (pY == nullptr) {
      errorCtx.setNullPointerError("DataConvert constructor", "output pointer pOut is null");
   }
   if (width == 0) {
      errorCtx.setInvalidParameterError("DataConvert constructor", "width must be greater than zero");
   }
   if (height == 0) {
      errorCtx.setInvalidParameterError("DataConvert constructor", "height must be greater than zero");
   }

   // Only proceed with initialization if all validation checks passed
   if (errorCtx.isSuccess()) {

      // Initialize bufParams with node input and output buffer parameters
      _nodeInfo.bufParams.inputDataType     = ElementDType<dTypeIn>::value;
      _nodeInfo.bufParams.isInputInterleave = 0;
      _nodeInfo.bufParams.widthIn[0]        = width;
      _nodeInfo.bufParams.heightIn[0]       = height;
      _nodeInfo.bufParams.inPitchBytes[0]   = strideInBytes;

      _nodeInfo.bufParams.outputDataType     = ElementDType<dTypeOut>::value;
      _nodeInfo.bufParams.isOutputInterleave = 0;
      _nodeInfo.bufParams.widthOut[0]        = width;
      _nodeInfo.bufParams.heightOut[0]       = height;
      _nodeInfo.bufParams.outPitchBytes[0]   = strideOutBytes;

      size_t widthBlock = 0;

      if ((std::is_same<float, dTypeOut>::value && std::is_same<int16_t, dTypeIn>::value) ||
          (std::is_same<float, dTypeOut>::value && std::is_same<int32_t, dTypeIn>::value) ||
          (std::is_same<int32_t, dTypeOut>::value && std::is_same<float, dTypeIn>::value)) {

         widthBlock = (_width + (static_cast<size_t>(c7x::max_simd<dTypeOut>::value) - 1U)) /
                      static_cast<size_t>(c7x::max_simd<dTypeOut>::value);
         _se0Param.ELETYPE   = c7x::se_eletype<vecIn>::value;
         _se0Param.VECLEN    = c7x::se_veclen<vecOut>::value;
         _se0Param.DIMFMT    = __SE_DIMFMT_3D;
         _se0Param.DECDIM1   = __SE_DECDIM_DIM1;
         _se0Param.DECDIM2   = __SE_DECDIM_DIM0;
         _se0Param.DECDIM1SD = __SE_DECDIMSD_DIM0;
         _se0Param.DECDIM2SD = __SE_DECDIMSD_DIM0;

         _se0Param.ICNT0         = static_cast<uint32_t>(c7x::max_simd<dTypeOut>::value);
         _se0Param.ICNT1         = static_cast<uint32_t>(widthBlock);
         _se0Param.DIM1          = static_cast<int32_t>(c7x::max_simd<dTypeOut>::value);
         _se0Param.DECDIM1_WIDTH = static_cast<uint32_t>(_width);
         _se0Param.ICNT2         = static_cast<uint32_t>(_height);
         _se0Param.DIM2          = static_cast<int32_t>(strideInBytes / sizeof(dTypeIn));

         if (std::is_same<float, dTypeOut>::value && std::is_same<int16_t, dTypeIn>::value) {
            _se0Param.PROMOTE = __SE_PROMOTE_2X_SIGNEXT;
         }

         _sa0Param.VECLEN    = c7x::sa_veclen<vecOut>::value;
         _sa0Param.DIMFMT    = __SA_DIMFMT_3D;
         _sa0Param.DECDIM1   = __SA_DECDIM_DIM1;
         _sa0Param.DECDIM2   = __SA_DECDIM_DIM0;
         _sa0Param.DECDIM1SD = __SA_DECDIMSD_DIM0;
         _sa0Param.DECDIM2SD = __SA_DECDIMSD_DIM0;

         _sa0Param.ICNT0         = static_cast<uint32_t>(c7x::max_simd<dTypeOut>::value);
         _sa0Param.ICNT1         = static_cast<uint32_t>(widthBlock);
         _sa0Param.DIM1          = static_cast<int32_t>(c7x::max_simd<dTypeOut>::value);
         _sa0Param.DECDIM1_WIDTH = static_cast<uint32_t>(width);
         _sa0Param.ICNT2         = static_cast<uint32_t>(height);
         _sa0Param.DIM2          = static_cast<int32_t>(strideOutBytes / sizeof(dTypeOut));
      }

      if (std::is_same<int16_t, dTypeOut>::value && std::is_same<float, dTypeIn>::value) {

         widthBlock = (width + (static_cast<size_t>(c7x::max_simd<dTypeOut>::value) - 1U)) /
                      static_cast<size_t>(c7x::max_simd<dTypeOut>::value);

         _se0Param.ELETYPE   = c7x::se_eletype<vecIn>::value;
         _se0Param.VECLEN    = c7x::se_veclen<vecIn>::value;
         _se0Param.DIMFMT    = __SE_DIMFMT_3D;
         _se0Param.DECDIM1   = __SE_DECDIM_DIM1;
         _se0Param.DECDIM2   = __SE_DECDIM_DIM0;
         _se0Param.DECDIM1SD = __SE_DECDIMSD_DIM0;
         _se0Param.DECDIM2SD = __SE_DECDIMSD_DIM0;

         _se0Param.ICNT0         = static_cast<uint32_t>(c7x::max_simd<dTypeIn>::value);
         _se0Param.ICNT1         = static_cast<uint32_t>(widthBlock);
         _se0Param.DIM1          = static_cast<int32_t>(c7x::max_simd<dTypeIn>::value * 2);
         _se0Param.DECDIM1_WIDTH = static_cast<uint32_t>(width);
         _se0Param.ICNT2         = static_cast<uint32_t>(height);
         _se0Param.DIM2          = static_cast<int32_t>(strideInBytes / sizeof(dTypeIn));

         _sa0Param.VECLEN    = c7x::sa_veclen<vecOut>::value;
         _sa0Param.DIMFMT    = __SA_DIMFMT_3D;
         _sa0Param.DECDIM1   = __SA_DECDIM_DIM1;
         _sa0Param.DECDIM2   = __SA_DECDIM_DIM0;
         _sa0Param.DECDIM1SD = __SA_DECDIMSD_DIM0;
         _sa0Param.DECDIM2SD = __SA_DECDIMSD_DIM0;

         _sa0Param.ICNT0         = static_cast<uint32_t>(c7x::max_simd<dTypeOut>::value);
         _sa0Param.ICNT1         = static_cast<uint32_t>(widthBlock);
         _sa0Param.DIM1          = static_cast<int32_t>(c7x::max_simd<dTypeOut>::value);
         _sa0Param.DECDIM1_WIDTH = static_cast<uint32_t>(width);
         _sa0Param.ICNT2         = static_cast<uint32_t>(height);
         _sa0Param.DIM2          = static_cast<int32_t>(strideOutBytes / sizeof(dTypeOut));
      }

      _computeLoopCount = widthBlock * height;
   }
}

namespace {

template <typename dTypeIn, typename dTypeOut>
std::enable_if_t<std::is_same<int16_t, dTypeOut>::value && std::is_same<float, dTypeIn>::value> inline process_data(
    dTypeOut *RESTRICT pY,
    size_t             computeLoopCount)
{

   const c7x::uchar_vec vpermReg = c7x::uchar_vec(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                                  20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);

   using vecOut = typename c7x::make_full_vector<dTypeOut>::type;
   for (size_t i = 0; i < computeLoopCount; i++) {
      c7x::int_vec dataIn0 = __float_to_int(c7x::strm_eng<0, c7x::float_vec>::get_adv());
      c7x::int_vec dataIn1 = __float_to_int(c7x::strm_eng<1, c7x::float_vec>::get_adv());

      vecOut dataOut = c7x::as_short_vec(
          __permute_even_even_short(vpermReg, c7x::as_uchar_vec(dataIn1), c7x::as_uchar_vec(dataIn0)));

      __vpred tmp  = c7x::strm_agen<0, vecOut>::get_vpred();
      vecOut *addr = c7x::strm_agen<0, vecOut>::get_adv(pY);
      __vstore_pred(tmp, addr, dataOut);
   }
}

template <typename dTypeIn, typename dTypeOut>
std::enable_if_t<std::is_same<float, dTypeOut>::value && std::is_same<int16_t, dTypeIn>::value> inline process_data(
    dTypeOut *RESTRICT pY,
    size_t             computeLoopCount)

{

   using vecOut = typename c7x::make_full_vector<dTypeOut>::type;
   for (size_t i = 0; i < computeLoopCount; i++) {
      vecOut  dataOut = __int_to_float(c7x::strm_eng<0, c7x::int_vec>::get_adv());
      __vpred tmp     = c7x::strm_agen<0, vecOut>::get_vpred();
      vecOut *addr    = c7x::strm_agen<0, vecOut>::get_adv(pY);
      __vstore_pred(tmp, addr, dataOut);
   }
}

template <typename dTypeIn, typename dTypeOut>
std::enable_if_t<std::is_same<float, dTypeOut>::value && std::is_same<int32_t, dTypeIn>::value> inline process_data(
    dTypeOut *RESTRICT pY,
    size_t             computeLoopCount)

{
   using vecIn  = typename c7x::make_full_vector<dTypeIn>::type;
   using vecOut = typename c7x::make_full_vector<dTypeOut>::type;
   for (size_t i = 0; i < computeLoopCount; i++) {
      vecOut dataOut = __int_to_float(c7x::strm_eng<0, vecIn>::get_adv()) *
                       ((vecOut) TISP_SCALE_INT32_TO_FLOAT); // NOLINT(MISRA.C_CAST)
      __vpred tmp  = c7x::strm_agen<0, vecOut>::get_vpred();
      vecOut *addr = c7x::strm_agen<0, vecOut>::get_adv(pY);
      __vstore_pred(tmp, addr, dataOut);
   }
}

template <typename dTypeIn, typename dTypeOut>
std::enable_if_t<std::is_same<int32_t, dTypeOut>::value && std::is_same<float, dTypeIn>::value> inline process_data(
    dTypeOut *RESTRICT pY,
    size_t             computeLoopCount)

{
   using vecIn  = typename c7x::make_full_vector<dTypeIn>::type;
   using vecOut = typename c7x::make_full_vector<dTypeOut>::type;

   for (size_t i = 0; i < computeLoopCount; i++) {
      vecIn dataIn = (c7x::strm_eng<0, vecIn>::get_adv()) * ((vecIn) TISP_SCALE_FLOAT_TO_INT32); // NOLINT(MISRA.C_CAST)
      vecOut  dataOut = __float_to_int(dataIn);
      __vpred tmp     = c7x::strm_agen<0, vecOut>::get_vpred();
      vecOut *addr    = c7x::strm_agen<0, vecOut>::get_adv(pY);
      __vstore_pred(tmp, addr, dataOut);
   }
}

} // anonymous namespace

template <typename dTypeIn, typename dTypeOut>
void DataConvert<dTypeIn, dTypeOut>::exec_ci(dTypeIn *RESTRICT pX, dTypeOut *RESTRICT pY)
{

   // open SE0 to fetch Input samples
   __SE0_OPEN(pX, _se0Param);

   if (std::is_same<int16_t, dTypeOut>::value && std::is_same<float, dTypeIn>::value) {

      size_t offset = c7x::max_simd<dTypeIn>::value;
      __SE1_OPEN(&(pX[offset]), _se0Param); // NOLINT(MISRA.PTR.ARITH)
   }

   // open SA0 to write output samples
   __SA0_OPEN(_sa0Param);

   process_data<dTypeIn, dTypeOut>(pY, _computeLoopCount);

   // close SE/SA
   __SE0_CLOSE();
   __SA0_CLOSE();
}

template <typename dTypeIn, typename dTypeOut> void DataConvert<dTypeIn, dTypeOut>::exec() noexcept
{
   exec_ci(_pX, _pY);
}

template <typename dTypeIn, typename dTypeOut>
void *DataConvert<dTypeIn, dTypeOut>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename dTypeIn, typename dTypeOut>
void *DataConvert<dTypeIn, dTypeOut>::getOutputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pY) : nullptr;
}

template <typename dTypeIn, typename dTypeOut> size_t DataConvert<dTypeIn, dTypeOut>::getNumInputs() const noexcept
{
   return 1;
}

template <typename dTypeIn, typename dTypeOut> size_t DataConvert<dTypeIn, dTypeOut>::getNumOutputs() const noexcept
{
   return 1;
}

template <typename dTypeIn, typename dTypeOut>
void DataConvert<dTypeIn, dTypeOut>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<dTypeIn *>(addr.src[0]);
   }
   if (mode == kAddrModeOut) {
      _pY = static_cast<dTypeOut *>(addr.dst[0]);
   }

   if (mode == kAddrModeInOut) {
      _pX = static_cast<dTypeIn *>(addr.src[0]);
      _pY = static_cast<dTypeOut *>(addr.dst[0]);
   }
}

template <typename dTypeIn, typename dTypeOut>
DataConvert<dTypeIn, dTypeOut>::DataConvert(dTypeIn    *pX,
                                            dTypeOut   *pY,
                                            size_t      width,
                                            size_t      height,
                                            size_t      strideInBytes,
                                            size_t      strideOutBytes,
                                            const char *nodeName,
                                            uint32_t    nodeId,
                                            ErrorCtxt  &errorCtx)
    : DataConvert(pX, pY, width, height, strideInBytes, strideOutBytes, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template class DataConvert<int16_t, float>;
template class DataConvert<float, int16_t>;
template class DataConvert<int32_t, float>;
template class DataConvert<float, int32_t>;

} // namespace misc
} // namespace TISP
