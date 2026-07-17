// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include "TISP_bump_allocator.hpp"
#include <cstdint>
#include <cstring>
#include <memory>

namespace TISP {

// ---------------------------------------------------------------------------
// BumpAllocatorDeleter
// ---------------------------------------------------------------------------

void BumpAllocatorDeleter::operator()(void *ptr)
{
   if (_alloc != nullptr) {
      _alloc->deallocate(ptr);
   }
}

// ---------------------------------------------------------------------------
// BumpAllocator — core
// ---------------------------------------------------------------------------

BumpAllocator::BumpAllocator(void *buffer, size_t size, size_t requiredAlignment)
    : _buffer(buffer), _size(((buffer != nullptr) && (size > 0U)) ? size : 0U), _offset(0), _allocatedMemoryBlocks()
{
   // Invalid args (null buffer or zero size) set _size to 0 so all subsequent
   // allocate() calls return nullptr gracefully — no assert() per MISRA C++:2023.
   if ((requiredAlignment > 0U) && (_size > 0U)) {
      void  *check = _buffer;
      size_t space = _size;
      if (std::align(requiredAlignment, 1U, check, space) != _buffer) {
         _size = 0U; // Misaligned buffer — disable arena gracefully
      }
   }
}

template <typename T, size_t Alignment> ArenaPtr<T> BumpAllocator::allocate(size_t count)
{
   if (count == 0U) {
      return ArenaPtr<T>(nullptr, *this);
   }

   size_t aligned_offset = (_offset + Alignment - 1U) & ~(Alignment - 1U);
   size_t padded_size    = (count * sizeof(T) + Alignment - 1U) & ~(Alignment - 1U);
   if (aligned_offset + padded_size > _size) {
      return ArenaPtr<T>(nullptr, *this);
   }

   void *ptr = &static_cast<char *>(_buffer)[aligned_offset];
   memset(ptr, 0, padded_size);
   _allocatedMemoryBlocks.push_back(std::make_pair(ptr, padded_size));
   _offset = aligned_offset + padded_size;

   return ArenaPtr<T>(static_cast<T *>(ptr), *this);
}

template <typename T> ArenaPtr<T> BumpAllocator::allocate(size_t count, size_t alignment)
{
   // Return nullptr for invalid alignment (zero or non-power-of-2) rather than
   // asserting — keeps the check unconditionally present in all build configs.
   if ((alignment == 0U) || ((alignment & (alignment - 1U)) != 0U)) {
      return ArenaPtr<T>(nullptr, *this);
   }

   if (count == 0U) {
      return ArenaPtr<T>(nullptr, *this);
   }

   // Use std::align to find the next aligned address (avoids reinterpret_cast).
   // Capture space before std::align so alignGap = spaceBefore - space (no pointer subtraction).
   void  *pool        = &static_cast<char *>(_buffer)[_offset];
   size_t spaceBefore = _size - _offset;
   size_t space       = spaceBefore;

   size_t padded_size = (count * sizeof(T) + alignment - 1U) & ~(alignment - 1U);

   if (std::align(alignment, padded_size, pool, space) == nullptr) {
      return ArenaPtr<T>(nullptr, *this);
   }

   size_t alignGap     = spaceBefore - space;
   size_t totalAdvance = alignGap + padded_size;

   memset(pool, 0, padded_size);
   _allocatedMemoryBlocks.push_back(std::make_pair(pool, totalAdvance));
   _offset += totalAdvance;

   return ArenaPtr<T>(static_cast<T *>(pool), *this);
}

void BumpAllocator::deallocate(void *ptr)
{
   auto it  = _allocatedMemoryBlocks.begin();
   auto end = _allocatedMemoryBlocks.end();

   for (; it != end; ++it) {
      if (it->first == ptr) {
         // Only reclaim offset if this is the top-of-stack (most recent) block.
         // Out-of-order deallocation removes tracking but leaves the space
         // consumed until reset() — this prevents offset corruption when
         // blocks are freed in non-LIFO order.
         if (it == std::prev(_allocatedMemoryBlocks.end())) {
            _offset -= it->second;
         }
         _allocatedMemoryBlocks.erase(it);
         return;
      }
   }
}

void BumpAllocator::reset()
{
   if (_offset > 0U) {
      std::memset(_buffer, 0, _offset);
   }
   _offset = 0;
   _allocatedMemoryBlocks.clear();
}

size_t BumpAllocator::getBufferSize() const { return _size; }

size_t BumpAllocator::getBytesUsed() const { return _offset; }

size_t BumpAllocator::getSize(void *ptr) const
{
   for (const auto &pair : _allocatedMemoryBlocks) {
      if (pair.first == ptr) {
         return pair.second;
      }
   }
   return 0;
}

bool BumpAllocator::isValid() const { return _size > 0U; }

// ---------------------------------------------------------------------------
// Convenience wrappers
// ---------------------------------------------------------------------------

template <typename T> ArenaPtr<T> BumpAllocator::allocate64ByteAligned(size_t count)
{
   return allocate<T, kTwiddleAlignment>(count);
}

template <typename T> ArenaPtr<T> BumpAllocator::allocate32ByteAligned(size_t count)
{
   return allocate<T, kSIMDAlignment>(count);
}

template <typename T> ArenaPtr<T> BumpAllocator::allocateDefaultAligned(size_t count)
{
   return allocate<T, alignof(T)>(count);
}

template <typename T> ArenaPtr<T> BumpAllocator::allocateCustomAligned(size_t count, size_t alignment)
{
   return allocate<T>(count, alignment);
}

// ---------------------------------------------------------------------------
// Explicit template instantiations
// Expanded from macro to avoid MISRA.DEFINE.NOPARS (type parameter cannot be
// parenthesized in template context; expanding eliminates the macro entirely).
// ---------------------------------------------------------------------------

template ArenaPtr<char>     BumpAllocator::allocate64ByteAligned<char>(size_t);
template ArenaPtr<char>     BumpAllocator::allocate32ByteAligned<char>(size_t);
template ArenaPtr<char>     BumpAllocator::allocateDefaultAligned<char>(size_t);
template ArenaPtr<char>     BumpAllocator::allocateCustomAligned<char>(size_t, size_t);

template ArenaPtr<int8_t>   BumpAllocator::allocate64ByteAligned<int8_t>(size_t);
template ArenaPtr<int8_t>   BumpAllocator::allocate32ByteAligned<int8_t>(size_t);
template ArenaPtr<int8_t>   BumpAllocator::allocateDefaultAligned<int8_t>(size_t);
template ArenaPtr<int8_t>   BumpAllocator::allocateCustomAligned<int8_t>(size_t, size_t);

template ArenaPtr<uint8_t> BumpAllocator::allocate64ByteAligned<uint8_t>(size_t);
template ArenaPtr<uint8_t> BumpAllocator::allocate32ByteAligned<uint8_t>(size_t);
template ArenaPtr<uint8_t> BumpAllocator::allocateDefaultAligned<uint8_t>(size_t);
template ArenaPtr<uint8_t> BumpAllocator::allocateCustomAligned<uint8_t>(size_t, size_t);

template ArenaPtr<int16_t> BumpAllocator::allocate64ByteAligned<int16_t>(size_t);
template ArenaPtr<int16_t> BumpAllocator::allocate32ByteAligned<int16_t>(size_t);
template ArenaPtr<int16_t> BumpAllocator::allocateDefaultAligned<int16_t>(size_t);
template ArenaPtr<int16_t> BumpAllocator::allocateCustomAligned<int16_t>(size_t, size_t);

template ArenaPtr<uint16_t> BumpAllocator::allocate64ByteAligned<uint16_t>(size_t);
template ArenaPtr<uint16_t> BumpAllocator::allocate32ByteAligned<uint16_t>(size_t);
template ArenaPtr<uint16_t> BumpAllocator::allocateDefaultAligned<uint16_t>(size_t);
template ArenaPtr<uint16_t> BumpAllocator::allocateCustomAligned<uint16_t>(size_t, size_t);

template ArenaPtr<int32_t> BumpAllocator::allocate64ByteAligned<int32_t>(size_t);
template ArenaPtr<int32_t> BumpAllocator::allocate32ByteAligned<int32_t>(size_t);
template ArenaPtr<int32_t> BumpAllocator::allocateDefaultAligned<int32_t>(size_t);
template ArenaPtr<int32_t> BumpAllocator::allocateCustomAligned<int32_t>(size_t, size_t);

template ArenaPtr<uint32_t> BumpAllocator::allocate64ByteAligned<uint32_t>(size_t);
template ArenaPtr<uint32_t> BumpAllocator::allocate32ByteAligned<uint32_t>(size_t);
template ArenaPtr<uint32_t> BumpAllocator::allocateDefaultAligned<uint32_t>(size_t);
template ArenaPtr<uint32_t> BumpAllocator::allocateCustomAligned<uint32_t>(size_t, size_t);

template ArenaPtr<int64_t> BumpAllocator::allocate64ByteAligned<int64_t>(size_t);
template ArenaPtr<int64_t> BumpAllocator::allocate32ByteAligned<int64_t>(size_t);
template ArenaPtr<int64_t> BumpAllocator::allocateDefaultAligned<int64_t>(size_t);
template ArenaPtr<int64_t> BumpAllocator::allocateCustomAligned<int64_t>(size_t, size_t);

template ArenaPtr<uint64_t> BumpAllocator::allocate64ByteAligned<uint64_t>(size_t);
template ArenaPtr<uint64_t> BumpAllocator::allocate32ByteAligned<uint64_t>(size_t);
template ArenaPtr<uint64_t> BumpAllocator::allocateDefaultAligned<uint64_t>(size_t);
template ArenaPtr<uint64_t> BumpAllocator::allocateCustomAligned<uint64_t>(size_t, size_t);

template ArenaPtr<float> BumpAllocator::allocate64ByteAligned<float>(size_t);
template ArenaPtr<float> BumpAllocator::allocate32ByteAligned<float>(size_t);
template ArenaPtr<float> BumpAllocator::allocateDefaultAligned<float>(size_t);
template ArenaPtr<float> BumpAllocator::allocateCustomAligned<float>(size_t, size_t);

template ArenaPtr<double> BumpAllocator::allocate64ByteAligned<double>(size_t);
template ArenaPtr<double> BumpAllocator::allocate32ByteAligned<double>(size_t);
template ArenaPtr<double> BumpAllocator::allocateDefaultAligned<double>(size_t);
template ArenaPtr<double> BumpAllocator::allocateCustomAligned<double>(size_t, size_t);

// void* used by Concat to store input pointer arrays
template ArenaPtr<void *>   BumpAllocator::allocateCustomAligned<void *>(size_t, size_t);

// Node arena uses allocate<uint8_t, kNodeAlignment> directly (not via convenience wrappers).
template ArenaPtr<uint8_t> BumpAllocator::allocate<uint8_t, kNodeAlignment>(size_t);

} // namespace TISP

// ---------------------------------------------------------------------------
// Application-provided arena buffers (extern — defined by the application)
// These symbols are at global scope; the application defines them.
// ---------------------------------------------------------------------------

extern uint8_t tisp_twiddleArenaBuffer[];
extern const size_t tisp_twiddleArenaSize;

extern uint8_t tisp_nodeArenaBuffer[];
extern const size_t tisp_nodeArenaSize;

extern uint8_t tisp_handleArenaBuffer[];
extern const size_t tisp_handleArenaSize;

namespace TISP {

// ---------------------------------------------------------------------------
// Library-owned twiddle-factor arena
// ---------------------------------------------------------------------------

static BumpAllocator twiddleAllocatorInstance(tisp_twiddleArenaBuffer, tisp_twiddleArenaSize, kTwiddleAlignment);

BumpAllocator &twiddleAllocator() { return twiddleAllocatorInstance; }

// ---------------------------------------------------------------------------
// Library-owned node arena for VirtualOp L2SRAM placement
// ---------------------------------------------------------------------------

static BumpAllocator nodeAllocatorInstance(tisp_nodeArenaBuffer, tisp_nodeArenaSize, kNodeAlignment);

BumpAllocator &nodeAllocator() { return nodeAllocatorInstance; }

// ---------------------------------------------------------------------------
// Library-owned handle arena
// ---------------------------------------------------------------------------

static BumpAllocator handleAllocatorInstance(tisp_handleArenaBuffer, tisp_handleArenaSize, kSIMDAlignment);

BumpAllocator &handleAllocator() { return handleAllocatorInstance; }

} // namespace TISP
