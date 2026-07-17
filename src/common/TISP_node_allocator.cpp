// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_node_allocator.hpp"
#include "TISP_virtualOp.hpp"

namespace TISP {

NodeDeleter::NodeDeleter() noexcept : _source(Source::kArena) {}

NodeDeleter::NodeDeleter(Source s) noexcept : _source(s) {}

NodeDeleter NodeDeleter::forArena() noexcept { return NodeDeleter{Source::kArena}; }

NodeDeleter NodeDeleter::forDDR() noexcept { return NodeDeleter{Source::kDDR}; }

void NodeDeleter::operator()(VirtualOp *ptr) noexcept
{
   if (ptr == nullptr) {
      return;
   }
   if (_source == Source::kDDR) {
      delete ptr;
   }
   else {
      ptr->~VirtualOp();
      nodeAllocator().deallocate(ptr);
   }
}

} // namespace TISP
