// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "../src/common/TISP_node_allocator.hpp"
#include "TISP_ErrorCtxt.hpp"
#include "TISP_types.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace TISP {

// Define a constant for maximum source addresses for a given node
constexpr int kMaxSrcAddr = 4;

// Define a constant for maximum destination addresses for a given node
constexpr int kMaxDstAddr = 3;

/**
 * @struct setAddr
 * @brief Structure to hold source and destination addresses
 */
typedef struct SetAddr {
   /**
    * @var src
    * @brief Array of source addresses
    */
   std::array<void *, kMaxSrcAddr> src;

   /**
    * @var dst
    * @brief Array of destination addresses
    */
   std::array<void *, kMaxDstAddr> dst;
} SetAddr_t;

// Define an enumeration for address modes
typedef enum SetAddrMode { kAddrModeIn = 0, kAddrModeOut, kAddrModeInOut } SetAddrMode_t;

/**
 * @struct NodeBufParams
 * @brief Aggregates the node's input and output buffer params that every
 *        VirtualOp-derived class carries.
 *
 * @details The per-buffer arrays are indexed by buffer position: input buffer
 *          @c i occupies index @c i (0 .. @ref kMaxSrcAddr - 1) and output
 *          buffer @c j occupies index @c j (0 .. @ref kMaxDstAddr - 1). Only
 *          the entries for the buffers a node actually exposes (see
 *          @ref VirtualOp::getNumInputs / @ref VirtualOp::getNumOutputs) are
 *          meaningful; the remaining entries are unused.
 */
struct NodeBufParams
{
   Types  inputDataType;                /**< @brief Element data type of the input buffer(s). */
   size_t isInputInterleave;            /**< @brief Input layout: 1 if channel-interleaved, 0 if non-interleaved. */
   size_t widthIn[kMaxSrcAddr];         /**< @brief Width (fastest-varying extent, in elements) of each input buffer. */
   size_t heightIn[kMaxSrcAddr];        /**< @brief Height (number of rows) of each input buffer. */
   size_t inPitchBytes[kMaxSrcAddr];    /**< @brief Row pitch (stride between rows, in bytes) of each input buffer. */

   Types  outputDataType;               /**< @brief Element data type of the output buffer(s). */
   size_t isOutputInterleave;           /**< @brief Output layout: 1 if channel-interleaved, 0 if non-interleaved. */
   size_t widthOut[kMaxDstAddr];        /**< @brief Width (fastest-varying extent, in elements) of each output buffer. */
   size_t heightOut[kMaxDstAddr];       /**< @brief Height (number of rows) of each output buffer. */
   size_t outPitchBytes[kMaxDstAddr];   /**< @brief Row pitch (stride between rows, in bytes) of each output buffer. */
};

/**
 * @struct NodeInfo
 * @brief Aggregates the node name, parameters string, and node ID that every
 *        VirtualOp-derived class carries.  Using std::array eliminates the
 *        C-style arrays that would otherwise trigger AUTOSAR.ARRAY.CSTYLE.
 */
struct NodeInfo {

   NodeBufParams bufParams{}; /*!< Input/output buffer geometry and data types */

   static constexpr uint32_t     kNameSize = 32U;
   static constexpr uint32_t     kParamsSize = 128U;
   std::array<char, kNameSize>   nodeName{};
   std::array<char, kParamsSize> params{};
   uint32_t                      nodeId{0U};
   const char                   *getNodeName() const noexcept { return nodeName.data(); }
   uint32_t                      getNodeId() const noexcept { return nodeId; }
   const char                   *getNodeParams() const noexcept { return params.data(); }
   void                          setName(const char *name) noexcept
   {
      if (name == nullptr) {
         return;
      }
      (void) std::strncpy(nodeName.data(), name, nodeName.size() - 1U);
      nodeName[nodeName.size() - 1U] = '\0';
   }
   const NodeBufParams &getNodeBufParams() const noexcept { return bufParams; }
};

/**
 * @class VirtualOp
 * @brief Abstract base class for operations (nodes)
 *
 * This class provides a common interface for different operations.
 * It declares pure virtual functions that derived classes must implement.
 */
class VirtualOp {
 public:
   /**
    * @brief Get the name of the node
    *
    * This function should be implemented by derived classes to return the node name.
    *
    * @retval const char* The name of the node
    * @throw None
    */
   virtual const char *getNodeName() const noexcept = 0;

   /**
    * @brief Get the ID of the node
    *
    * This function should be implemented by derived classes to return the node ID.
    *
    * @retval uint32_t The ID of the node
    * @throw None
    */
   virtual uint32_t getNodeId() const noexcept = 0;

   /**

    * @brief Get the parameters of the node as string
    *
    * This function should be implemented by derived classes to return the node parameters.
    *
    * @retval const char* The parameters of the node
    * @throw None
    */
   virtual const char *getNodeParams() const noexcept = 0;

   /**

    * @brief Get the input and output buffer parameters of the node
    *
    * This function should be implemented by derived classes to return the node parameters.
    *
    * @retval const NodeBufParams& : A constant reference to the NodeBufParams representing node's i/o buffer properties
    * @throw None
    */
   virtual const NodeBufParams &getNodeBufParams() const noexcept = 0;

   /**
    * @brief Execute the operation
    *
    * This function should be implemented by any concrete operation class.
    * It is intended to perform the actual operation.
    *
    * @retval None
    * @throw None
    */
   virtual void exec() noexcept = 0;

   /**
    * @brief Get the address of an input buffer
    *
    * @param[in]  index : Index of the input buffer
    * @retval     void* : Pointer to the input buffer, or nullptr if invalid
    * @throw None
    *
    * @note Default implementation returns nullptr (for nodes without indexed inputs)
    */
   virtual void *getInputAddr(size_t index) const noexcept
   {
      (void) index;
      return nullptr;
   }

   /**
    * @brief Get the address of an output buffer
    *
    * @param[in]  index : Index of the output buffer
    * @retval     void* : Pointer to the output buffer, or nullptr if invalid
    * @throw None
    *
    * @note Default implementation returns nullptr (for nodes without indexed outputs)
    */
   virtual void *getOutputAddr(size_t index) const noexcept
   {
      (void) index;
      return nullptr;
   }

   /**
    * @brief Get the number of input buffers
    *
    * @retval    size_t : The number of input buffers
    * @throw None
    *
    * @note Default implementation returns 0
    */
   virtual size_t getNumInputs() const noexcept { return 0; }

   /**
    * @brief Get the number of output buffers
    *
    * @retval    size_t : The number of output buffers
    * @throw None
    *
    * @note Default implementation returns 0
    */
   virtual size_t getNumOutputs() const noexcept { return 0; }

   /**
    * @brief Set runtime parameters for a node (with error context)
    * @param paramId Identifier for the parameter
    * @param value Pointer to parameter value
    * @param size Size of value in bytes
    * @param errorCtx Error context for reporting errors
    * @return true on success, false on validation failure
    *
    * @note Default implementation returns true (no-op)
    */
   virtual bool setParams(uint16_t paramId, void *value, size_t size, ErrorCtxt &errorCtx) noexcept
   {
      (void) paramId;
      (void) value;
      (void) size;
      (void) errorCtx;
      return true;
   }

   /**
    * @brief Get runtime parameters from a node (with error context)
    * @param paramId Identifier for the parameter
    * @param value Pointer to parameter value
    * @param size Pointer to size of value (in/out)
    * @param errorCtx Error context for reporting errors
    * @return true on success, false on failure
    *
    * @note Default implementation returns true (no-op)
    */
   virtual bool getParams(uint16_t paramId, void *value, size_t *size, ErrorCtxt &errorCtx) noexcept
   {
      (void) paramId;
      (void) value;
      (void) size;
      (void) errorCtx;
      return true;
   }

   /**
    * @brief Set source and destination addresses for the operation
    *
    * This function should be implemented by any concrete operation class.
    * It is intended to set the source and destination addresses for the operation.
    *
    * @param[in] addrs Structure containing source and destination addresses
    * @param[in] mode Mode of address setting (input, output, or both)
    * @retval None
    * @throw None
    */
   virtual void setAddr(SetAddr_t &addrs, SetAddrMode_t mode) noexcept = 0;

   /**
    * @brief Clean and reset all internal state and history buffers to zero
    *
    * This function should be implemented by any concrete operation class that maintains
    * internal state buffers or history buffers. It is called to reset the node state
    * before first execution and when runtime parameters (e.g., filter coefficients) are updated.
    *
    * @retval None
    * @throw None
    */
   virtual void cleanInternalBuffers() noexcept
   {
      return;
   };

   /**
    * @brief Virtual destructor
    *
    * This destructor is declared as virtual to ensure proper destruction of derived class objects.
    *
    * @retval None
    * @throw None
    */
   virtual ~VirtualOp() noexcept = default;
};

/// Backward-compatible graph container that holds VirtualOp nodes allocated
/// in either L2SRAM (via make_unique_node) or DDR (via std::make_unique).
///
/// Both allocation paths coexist in the same vector. The NodeDeleter
/// embedded in each unique_ptr routes destruction correctly:
///   - kArena: explicit dtor + deallocate to bump allocator (target)
///   - kDDR:   standard delete (backward compat for std::make_unique nodes)
///
/// Old code using std::make_unique<Node>() + push_back compiles unchanged —
/// the overloaded push_back adopts the node with a kDDR deleter.
class opVec {
   using container_t = std::vector<NodePtr>;
   container_t _nodes;

 public:
   /// Push an L2-allocated node (from make_unique_node).
   /// Callers must ensure make_unique_node returned a non-null pointer before
   /// calling push_back — a null node is silently ignored to keep the check
   /// unconditionally present in all build configurations (MISRA C++:2023).
   void push_back(NodePtr &&node)
   {
      if (node == nullptr) {
         return; // Arena exhausted: node not added; graph will be incomplete.
      }
      _nodes.push_back(std::move(node));
   }

   /// Push a DDR-allocated node (from std::make_unique — backward compat).
   /// Transfers ownership: node.release() -> re-owned by NodePtr with kDDR deleter.
   void push_back(std::unique_ptr<VirtualOp> &&node) { _nodes.emplace_back(node.release(), NodeDeleter::forDDR()); }

   // Standard container interface (C++14 trailing return types)
   auto begin() const noexcept -> container_t::const_iterator { return _nodes.begin(); }
   auto end() const noexcept -> container_t::const_iterator { return _nodes.end(); }
   auto begin() noexcept -> container_t::iterator { return _nodes.begin(); }
   auto end() noexcept -> container_t::iterator { return _nodes.end(); }

   std::size_t size() const noexcept { return _nodes.size(); }
   bool        empty() const noexcept { return _nodes.empty(); }

   NodePtr       &operator[](std::size_t i) { return _nodes[i]; }
   NodePtr const &operator[](std::size_t i) const { return _nodes[i]; }
};

} // namespace TISP
