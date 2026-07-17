// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace TISP {

class BumpAllocator; // Forward declaration

/**
 * @brief Custom deleter for memory managed by BumpAllocator.
 *
 * Invokes BumpAllocator::deallocate() when the owning unique_ptr is destroyed.
 *
 * @warning The BumpAllocator referenced by this deleter must outlive all ArenaPtr
 * instances created from it. This is guaranteed for the library-owned twiddle arena
 * (static lifetime) and for application-owned arenas declared as static arrays.
 * Do not create an BumpAllocator on the stack and return ArenaPtr instances from
 * that scope.
 */
class BumpAllocatorDeleter {
 public:
   BumpAllocatorDeleter() : _alloc(nullptr) {}
   BumpAllocatorDeleter(BumpAllocator &alloc) : _alloc(&alloc) {}
   void operator()(void *ptr);

 private:
   BumpAllocator *_alloc;
};

/**
 * @brief RAII-managed pointer to an array allocated from BumpAllocator.
 */
template <typename T> using ArenaPtr = std::unique_ptr<T[], BumpAllocatorDeleter>;

/**
 * @brief Semantic alias for ArenaPtr used for kernel handle allocations.
 *
 * Identical type to ArenaPtr<T> — use HandlePtr<T> for kernel handle members
 * to make ownership intent clear at the call site.
 */
template <typename T> using HandlePtr = ArenaPtr<T>;

/**
 * @brief Semantic alias for ArenaPtr used for twiddle/split-factor buffer allocations.
 *
 * Identical type to ArenaPtr<T> — use TwiddlePtr<T> for twiddle buffer members
 * to make ownership intent clear at the call site.
 */
template <typename T> using TwiddlePtr = ArenaPtr<T>;

/**
 * @brief Deterministic bump-pointer allocator over a caller-supplied backing array.
 *
 * Operates over a caller-supplied backing array — the memory section (L2SRAM,
 * DDR, or any other region) is determined by the caller at construction time.
 * The allocator increments an internal offset for each allocation and supports
 * configurable alignment (32-byte, 64-byte, 128-byte, or custom). Deallocation
 * releases memory in a best-effort fashion; reset() reclaims all memory at once.
 *
 * Two intended arena instances:
 *   - Node arena   (application-owned): app provides the backing array
 *   - Twiddle arena (library-owned):    internal static array for FFT coefficients
 *
 * Compliant with MISRA C++:2023 Rule 21.6.2, AUTOSAR A18-5-10, SEI CERT MEM54-CPP.
 *
 * @warning Lifetime: the allocator must outlive all ArenaPtr instances it creates.
 * Use static or program-scope backing arrays — do not allocate on the stack in a
 * function that returns ArenaPtr to the caller.
 */
class BumpAllocator {
 public:
   /**
    * @brief Construct an allocator over an existing buffer.
    *
    * @param buffer  Pointer to the backing array. The memory section (L2SRAM, DDR,
    *                or any other region) is chosen by the caller.
    * @param size    Size of the backing array in bytes.
    * @param requiredAlignment  If non-zero, validates that buffer meets this alignment.
    *                           Disables the arena (_size = 0) if misaligned.
    */
   BumpAllocator(void *buffer, size_t size, size_t requiredAlignment = 0U);

   /**
    * @brief Allocate a contiguous block with compile-time alignment.
    *
    * @tparam T         Element type.
    * @tparam Alignment  Required alignment in bytes (must be power of 2).
    * @param count  Number of elements of type T to allocate.
    * @return ArenaPtr<T>  RAII-managed pointer, or nullptr on failure.
    */
   template <typename T, size_t Alignment> ArenaPtr<T> allocate(size_t count);

   /**
    * @brief Allocate a contiguous block with runtime alignment.
    *
    * @tparam T  Element type.
    * @param count      Number of elements of type T to allocate.
    * @param alignment  Required alignment in bytes (must be power of 2).
    * @return ArenaPtr<T>  RAII-managed pointer, or nullptr on failure.
    */
   template <typename T> ArenaPtr<T> allocate(size_t count, size_t alignment);

   /** @brief Allocate with 64-byte alignment. */
   template <typename T> ArenaPtr<T> allocate64ByteAligned(size_t count);

   /** @brief Allocate with 32-byte alignment. */
   template <typename T> ArenaPtr<T> allocate32ByteAligned(size_t count);

   /** @brief Allocate with natural alignment of T. */
   template <typename T> ArenaPtr<T> allocateDefaultAligned(size_t count);

   /** @brief Allocate with caller-specified alignment. */
   template <typename T> ArenaPtr<T> allocateCustomAligned(size_t count, size_t alignment);

   /**
    * @brief Return memory to the allocator (best-effort offset adjustment).
    * @param ptr  Pointer previously returned by an allocate method.
    */
   void deallocate(void *ptr);

   /** @brief Reclaim all allocated memory at once. */
   void reset();

   /** @brief Total size of the backing buffer in bytes. */
   size_t getBufferSize() const;

   /** @brief Number of bytes consumed so far (current bump offset). */
   size_t getBytesUsed() const;

   /** @brief Padded size of the allocation that starts at ptr, or 0 if not found. */
   size_t getSize(void *ptr) const;

   /** @brief Returns true if the arena has a valid (non-zero) backing buffer. */
   bool isValid() const;

 private:
   void                                  *_buffer;
   size_t                                 _size;
   size_t                                 _offset;
   /** @note Uses heap-allocated std::vector for allocation tracking; migration to a
    *        fixed-size array is deferred to avoid churning all opVec call sites. */
   std::vector<std::pair<void *, size_t>> _allocatedMemoryBlocks;
};

/**
 * @brief Minimum SIMD alignment (bytes) for DSP vector operations.
 *
 * C7504/C7524: 32-byte SIMD width — minimum alignment for vector loads/stores.
 * Used by allocate32ByteAligned() convenience wrapper.
 */
static constexpr size_t kSIMDAlignment = 32U;

/**
 * @brief Required alignment (bytes) for twiddle / split-factor buffers.
 *
 * C7100/C7120: 64-byte SIMD — 64-byte alignment required.
 * C7504/C7524: 32-byte SIMD — 64-byte alignment used for safety margin.
 *
 * Change this single value if a future C7x core requires wider alignment.
 */
static constexpr size_t kTwiddleAlignment = 64U;

/**
 * @brief Alignment for VirtualOp node objects in the node arena.
 *
 * 128-byte alignment ensures cache-line alignment on C7x and satisfies
 * the strictest DMA transfer alignment requirements regardless of the
 * backing memory section chosen by the caller.
 *
 * Change this single value if a future C7x core requires wider alignment.
 */
static constexpr size_t kNodeAlignment = 128U;

/**
 * @brief Accessor for the twiddle-factor arena allocator.
 *
 * Returns a reference to a BumpAllocator backed by the application-provided
 * tisp_twiddleArenaBuffer[]. The application defines buffer size and section
 * placement at link time.
 */
BumpAllocator &twiddleAllocator();

/**
 * @brief Accessor for the node arena allocator.
 *
 * Returns a reference to a BumpAllocator backed by the application-provided
 * tisp_nodeArenaBuffer[]. The application defines buffer size and section
 * placement at link time.
 */
BumpAllocator &nodeAllocator();

/**
 * @brief Accessor for the handle arena allocator.
 *
 * Returns a reference to a BumpAllocator backed by the application-provided
 * tisp_handleArenaBuffer[]. The application defines buffer size and section
 * placement at link time.
 */
BumpAllocator &handleAllocator();

/**
 * @brief Allocate an array of T from the handle arena with SIMD alignment.
 *
 * Convenience wrapper around handleAllocator().allocateCustomAligned<T>().
 * Returns an ArenaPtr<T> (nullptr on arena exhaustion — caller must check).
 *
 * @tparam T      Element type.
 * @param  count  Number of elements to allocate.
 * @return ArenaPtr<T> RAII-managed pointer into the handle arena.
 */
template <typename T> HandlePtr<T> make_unique_handle(size_t count)
{
   return handleAllocator().allocateCustomAligned<T>(count, kSIMDAlignment);
}

} // namespace TISP
