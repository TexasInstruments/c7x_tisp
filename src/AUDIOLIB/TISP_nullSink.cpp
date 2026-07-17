// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_AUDIOLIB_common.hpp"
#include "TISP_virtualOp.hpp"
#include "common/AUDIOLIB_bufParams.h"
#include <AUDIOLIB/TISP_AUDIOLIB_types.hpp>
#include <TISP_nullSink.hpp>

namespace TISP {
namespace AUDIOLIB {

//! [TISP::Nullsink constructor]

template <typename T> Nullsink<T>::Nullsink(T *pX, ErrorCtxt &errorCtx) : _pX(pX)
{
   // Input validation checks
   if (pX == nullptr) {
      errorCtx.setNullPointerError("Nullsink constructor", "Input pointer pX is null");
   }
   else {
      // Nullsink discards its input and produces no output. The constructor is not given
      // any width/height/pitch for pX, so there is no real geometry to report here; only
      // the input element type is meaningful. Leave the dimension/pitch arrays and the
      // output side untouched rather than inventing fake dimensions.
      _nodeInfo.bufParams.inputDataType = ElementDType<T>::value;
   }
}

template <typename T>
Nullsink<T>::Nullsink(T *pX, const char *nodeName, uint32_t nodeId, ErrorCtxt &errorCtx) : Nullsink(pX, errorCtx)
{
   _nodeInfo.setName(nodeName);
   _nodeInfo.nodeId = nodeId;
}

template <typename T> void *Nullsink<T>::getInputAddr(size_t index) const noexcept
{
   return (index == 0) ? static_cast<void *>(_pX) : nullptr;
}

template <typename T> size_t Nullsink<T>::getNumInputs() const noexcept { return 1; }

//! [TISP::Nullsink setAddr]
template <typename T> void Nullsink<T>::setAddr(SetAddr_t &addr, SetAddrMode_t mode) noexcept
{
   if (mode == kAddrModeIn) {
      _pX = static_cast<T *>(addr.src[0]);
   }
}

template <typename T> void Nullsink<T>::exec() noexcept
{
   // No operation is performed
}

template class Nullsink<float>;
template class Nullsink<double>;
template class Nullsink<int8_t>;
template class Nullsink<uint8_t>;
template class Nullsink<int16_t>;
template class Nullsink<uint16_t>;
template class Nullsink<int32_t>;
template class Nullsink<uint32_t>;
template class Nullsink<int64_t>;
template class Nullsink<uint64_t>;

} // namespace AUDIOLIB
} // namespace TISP
