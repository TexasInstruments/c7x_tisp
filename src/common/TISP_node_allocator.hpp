// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_bump_allocator.hpp"
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace TISP {

class VirtualOp; // forward

/**
 * @class NodeDeleter
 * @brief Custom deleter for VirtualOp nodes allocated from an arena or DDR.
 *
 * Tracks where the node was allocated (arena vs DDR heap) and routes
 * destruction accordingly:
 *   - Arena: explicit ~VirtualOp() + nodeAllocator().deallocate()
 *   - DDR:   standard delete (backward-compat for std::make_unique nodes)
 *
 * Compliant with MISRA C++:2023 Rule 21.6.4 (ownership via smart pointers).
 */
class NodeDeleter {
 public:
   /// Default: arena-allocated node (used by make_unique_node).
   NodeDeleter() noexcept;

   /// Named constructor for arena-allocated nodes (used by make_unique_node).
   static NodeDeleter forArena() noexcept;

   /// Named constructor for DDR-allocated nodes (backward-compat path).
   static NodeDeleter forDDR() noexcept;

   /// Destroy and deallocate the pointed-to VirtualOp.
   void operator()(VirtualOp *ptr) noexcept;

 private:
   enum class Source : uint8_t { kArena = 0, kDDR = 1 };
   Source _source;

   explicit NodeDeleter(Source s) noexcept;
};

/// RAII-managed pointer to a VirtualOp allocated in L2SRAM (or DDR).
using NodePtr = std::unique_ptr<VirtualOp, NodeDeleter>;

/**
 * @brief Factory: placement-new a VirtualOp-derived T into the node arena.
 *
 * Uses nodeAllocator() (BumpAllocator) for the backing memory.
 * Returns unique_ptr<T, NodeDeleter> — implicitly convertible to
 * NodePtr via opVec::push_back.
 *
 * Compliant with MISRA C++:2023 Rule 21.6.2 (RAII for dynamic memory),
 * AUTOSAR C++14 A18-5-10 (pool allocators with placement new),
 * SEI CERT MEM54-CPP (custom allocators).
 */
template <typename T, typename... Args> std::unique_ptr<T, NodeDeleter> make_unique_node(Args &&...args)
{
   static_assert(std::is_base_of<VirtualOp, T>::value, "make_unique_node<T>: T must derive from VirtualOp");
   // Allocate raw memory from the node arena (static buffer on both PC and target).
   auto mem = nodeAllocator().allocate<uint8_t, kNodeAlignment>(sizeof(T));
   if (mem.get() == nullptr) {
      // Arena exhausted: return null pointer (caller must check before use).
      return std::unique_ptr<T, NodeDeleter>(nullptr, NodeDeleter::forArena());
   }
   T *p = new (mem.release()) T(std::forward<Args>(args)...);
   return std::unique_ptr<T, NodeDeleter>(p, NodeDeleter::forArena());
}

} // namespace TISP
