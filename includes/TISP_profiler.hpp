// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "TISP_superNode.hpp"
#include "TISP_virtualOp.hpp"
#include <c7x.h>

#include <memory>
#include <vector>

/**
 * @brief Maximum number of node timing entries to store
 *
 * This value can be overridden by defining TISP_MAX_NODE_PROFILE_ENTRIES before including this header.
 */
#ifndef TISP_MAX_NODE_PROFILE_ENTRIES
#define TISP_MAX_NODE_PROFILE_ENTRIES 500
#endif

/**
 * @brief Maximum number of supernode timing entries to store
 *
 * This value can be overridden by defining TISP_MAX_SUPERNODE_PROFILE_ENTRIES before including this header.
 */
#ifndef TISP_MAX_SUPERNODE_PROFILE_ENTRIES
#define TISP_MAX_SUPERNODE_PROFILE_ENTRIES 20
#endif

/**
 * @brief Macro for reading cycle counter
 *
 * In host emulation mode, this returns 0. In hardware mode, it reads the cycle counter.
 */
#ifndef HOST_EMULATION
#define TISP_PROFILE_READ (__get_GTSC(1), __STSC)
#else
#define TISP_PROFILE_READ (0)
#endif

/**
 * @struct NodeCycleStats
 * @brief Aggregated cycle statistics for a single profiled node
 *
 * Returned by NodeProfiler::getNodeCycles(). The @c current field is the
 * instantaneous measurement from the most recent execution, while @c max,
 * @c avg and @c count accumulate across every execution since the last
 * NodeProfiler::clear() or NodeProfiler::clearStats() call. They therefore
 * survive the NodeProfiler::resetCounters() that begins each graph execution.
 */
struct NodeCycleStats {
   uint64_t current; ///< Cycles consumed by the most recent execution of this node
   uint64_t max;     ///< Largest cycle count observed for this node since the last stats reset
   uint64_t avg;     ///< Mean cycles per execution (totalCycles / count); 0 when count is 0
   uint64_t count;   ///< Number of executions folded into @c max and @c avg
};

/**
 * @class NodeProfiler
 * @brief Performance profiling system for TISP nodes and supernodes
 *
 * The NodeProfiler class provides comprehensive performance profiling capabilities
 * for TISP nodes and supernodes. It captures cycle counts for each node execution,
 * tracks relationships between nodes and supernodes, and generates detailed
 * performance reports in various formats.
 *
 * Key features:
 * - Cycle count measurement for individual nodes and supernodes
 * - Node identification via nodeName and nodeId
 * - Block count tracking for supernodes
 * - JSON output for detailed performance analysis
 * - Support for hierarchical node relationships
 */
class NodeProfiler {
 private:
   /** @name Node tracking arrays and counters */
   ///@{
   static uint64_t nodeStartCycles[TISP_MAX_NODE_PROFILE_ENTRIES];       ///< Start cycle timestamps for each node
   static uint64_t nodeCycleCounts[TISP_MAX_NODE_PROFILE_ENTRIES];       ///< Cycle counts for each node
   static uint32_t nodeIds[TISP_MAX_NODE_PROFILE_ENTRIES];               ///< IDs for each node
   static uint32_t nodeParentSuperNodeId[TISP_MAX_NODE_PROFILE_ENTRIES]; ///< Which supernode contains this node
   static uint32_t nodeCount;                                            ///< Current number of nodes being tracked
   ///@}

   /** @name Node cycle statistics accumulated across executions
    *
    *  These are deliberately NOT cleared by resetCounters(), so they accumulate
    *  over every graph execution until clear() or clearStats() is called.
    */
   ///@{
   static uint64_t nodeMaxCycles[TISP_MAX_NODE_PROFILE_ENTRIES];   ///< Largest cycle count observed per node
   static uint64_t nodeTotalCycles[TISP_MAX_NODE_PROFILE_ENTRIES]; ///< Running sum of cycle counts per node
   static uint64_t nodeExecCount[TISP_MAX_NODE_PROFILE_ENTRIES];   ///< Number of executions recorded per node
   ///@}

   /** @name Supernode tracking arrays and counters */
   ///@{
   static uint64_t
       superNodeStartCycles[TISP_MAX_SUPERNODE_PROFILE_ENTRIES]; ///< Start cycle timestamps for each supernode
   static uint64_t superNodeCycleCounts[TISP_MAX_SUPERNODE_PROFILE_ENTRIES]; ///< Cycle counts for each supernode
   static uint32_t superNodeIds[TISP_MAX_SUPERNODE_PROFILE_ENTRIES];         ///< IDs for each supernode
   static uint32_t superNodeCount; ///< Current number of supernodes being tracked
   ///@}

   /** @name Current execution context */
   ///@{
   static uint32_t currentSuperNodeId; ///< ID of the currently executing supernode
   static bool     inSuperNode;        ///< Whether execution is currently inside a supernode
   ///@}

   /** @name Common data */
   ///@{
   static uint64_t overhead; ///< Profiling overhead in cycles
   ///@}

   /**
    * @brief Calculate profiling overhead
    *
    * Measures the overhead of reading the cycle counter twice in succession.
    * This overhead is subtracted from all timing measurements.
    */
   static inline void calculateOverhead()
   {
      uint64_t start = TISP_PROFILE_READ;
      uint64_t end   = TISP_PROFILE_READ;
      overhead       = end - start;
   }

 public:
   /**
    * @brief Initialize the profiler
    *
    * Calculates the profiling overhead and clears all timing data.
    * This should be called before any profiling operations.
    */
   static inline void initialize()
   {
      // Create a temporary instance to trigger the constructor
      calculateOverhead();
      NodeProfiler::clear();
   }

   /**
    * @brief Prepare for node timing
    *
    * Sets up the node ID and parent supernode ID for the current node.
    * This is called automatically by startNode().
    */
   static inline void prepareNode()
   {
      // Use nodeCount as the ID
      nodeIds[nodeCount] = nodeCount;
      // Track if this node is inside a supernode
      nodeParentSuperNodeId[nodeCount] = inSuperNode ? currentSuperNodeId : 0;
   }

   /**
    * @brief Start timing for a node
    *
    * Captures the start cycle count for the current node.
    * This is called automatically by the TISP framework before executing a node.
    */
   static inline void startNode()
   {
      if (nodeCount < TISP_MAX_NODE_PROFILE_ENTRIES) {
         // Embed prepareNode code directly
         // Use nodeCount as the ID
         nodeIds[nodeCount] = nodeCount;
         // Track if this node is inside a supernode
         nodeParentSuperNodeId[nodeCount] = inSuperNode ? currentSuperNodeId : 0;

         nodeStartCycles[nodeCount] = TISP_PROFILE_READ;
      }
      else {
         /* Nothing to do here - the node arrays are full */
      }
   }

   /**
    * @brief Stop timing for a node
    *
    * Captures the end cycle count for the current node, calculates the elapsed
    * cycles, folds the measurement into this node's running statistics (last,
    * maximum, total and execution count) and increments the node counter.
    * This is called automatically by the TISP framework after executing a node.
    */
   static inline void stopNode()
   {
      uint64_t endCycles = TISP_PROFILE_READ;

      if (nodeCount < TISP_MAX_NODE_PROFILE_ENTRIES) {
         // Saturate at zero rather than wrapping: a measurement shorter than the
         // calibrated overhead would otherwise underflow to a huge value and latch
         // itself permanently into nodeMaxCycles.
         uint64_t rawCycles = endCycles - nodeStartCycles[nodeCount];
         uint64_t elapsed   = (rawCycles > overhead) ? (rawCycles - overhead) : 0U;

         nodeCycleCounts[nodeCount] = elapsed;

         if (elapsed > nodeMaxCycles[nodeCount]) {
            nodeMaxCycles[nodeCount] = elapsed;
         }
         else {
            /* Nothing to do here */
         }

         nodeTotalCycles[nodeCount] += elapsed;
         nodeExecCount[nodeCount]++;

         nodeCount++;
      }
      else {
         /* Nothing to do here - the node arrays are full */
      }
   }

   /**
    * @brief Prepare for supernode timing
    *
    * Sets up the supernode ID and marks execution as being inside a supernode.
    * This is called automatically by startSuperNode().
    */
   static inline void prepareSuperNode()
   {
      // Use superNodeCount as the ID
      superNodeIds[superNodeCount] = superNodeCount;
      currentSuperNodeId           = superNodeCount;
      inSuperNode                  = true;
   }

   /**
    * @brief Start timing for a supernode
    *
    * Prepares the supernode and captures the start cycle count.
    * This is called automatically by the TISP framework before executing a supernode.
    */
   static inline void startSuperNode()
   {
      // Prepare the supernode
      prepareSuperNode();
      superNodeStartCycles[superNodeCount] = TISP_PROFILE_READ;
   }

   /**
    * @brief Stop timing for a supernode
    *
    * Captures the end cycle count for the current supernode, calculates the
    * elapsed cycles, increments the supernode counter, and marks execution
    * as no longer being inside a supernode.
    * This is called automatically by the TISP framework after executing a supernode.
    */
   static inline void stopSuperNode()
   {
      uint64_t endCycles                   = TISP_PROFILE_READ;
      superNodeCycleCounts[superNodeCount] = endCycles - superNodeStartCycles[superNodeCount] - overhead;
      superNodeCount++;
      inSuperNode = false;
   }

   /**
    * @brief Clear all timing data
    *
    * Resets all profiling data to zero, including node and supernode
    * counters, cycle counts, and execution context.
    */
   static inline void clear()
   {
      std::memset(nodeStartCycles, 0, sizeof(nodeStartCycles));
      std::memset(nodeCycleCounts, 0, sizeof(nodeCycleCounts));
      std::memset(nodeIds, 0, sizeof(nodeIds));
      std::memset(nodeParentSuperNodeId, 0, sizeof(nodeParentSuperNodeId));
      std::memset(nodeMaxCycles, 0, sizeof(nodeMaxCycles));
      std::memset(nodeTotalCycles, 0, sizeof(nodeTotalCycles));
      std::memset(nodeExecCount, 0, sizeof(nodeExecCount));
      nodeCount = 0;

      std::memset(superNodeStartCycles, 0, sizeof(superNodeStartCycles));
      std::memset(superNodeCycleCounts, 0, sizeof(superNodeCycleCounts));
      std::memset(superNodeIds, 0, sizeof(superNodeIds));
      superNodeCount = 0;

      currentSuperNodeId = 0;
      inSuperNode        = false;
   }

   /**
    * @brief Clear only the accumulated cycle statistics
    *
    * Resets the per-node maximum, total and execution count without disturbing
    * the node/supernode indices or the
    * most recent per-node measurement. Use this to start a fresh statistics
    * window - for example when a host tool asks for the peak load to be re-armed
    * after a parameter change - without re-initializing the profiler.
    */
   static inline void clearStats()
   {
      std::memset(nodeMaxCycles, 0, sizeof(nodeMaxCycles));
      std::memset(nodeTotalCycles, 0, sizeof(nodeTotalCycles));
      std::memset(nodeExecCount, 0, sizeof(nodeExecCount));
   }

   /**
    * @brief Reset node counters before executing a graph
    *
    * Resets both node and supernode counters to zero.
    * This allows multiple calls to execute::graph to reuse the same node IDs.
    *
    * @remarks This deliberately leaves the accumulated statistics (maximum,
    *          total, execution count) intact so that they build up across graph
    *          executions. Use clearStats() or clear() to reset those.
    */
   static inline void resetCounters()
   {
      nodeCount      = 0;
      superNodeCount = 0;
   }

   /**
    * @brief Reset only the node counter
    *
    * Resets just the node counter without affecting the supernode counter.
    * This is used by supernodes to avoid double counting nodes.
    */
   static inline void resetNodeCounter() { nodeCount = 0; }

   /**
    * @brief Get the start node ID for profiling
    *
    * Returns the current node count, which can be used as the starting ID
    * for a new set of nodes. This is used by supernodes for internal profiling.
    *
    * @return The current node count
    */
   static inline uint32_t getStartNodeId() { return nodeCount; }

   /**
    * @brief Set the start node ID
    *
    * Sets the node counter to the specified value.
    * This is used by supernodes to update the count after execution.
    *
    * @param id The new node count value
    */
   static inline void setStartNodeId(uint32_t id) { nodeCount = id; }

   /**
    * @brief Check if currently inside a supernode
    *
    * Returns whether execution is currently inside a supernode.
    *
    * @return true if inside a supernode, false otherwise
    */
   static inline bool isInSuperNode() { return inSuperNode; }

   // =========================================================================
   //
   //  These methods expose the cycle data captured during the last
   //  execute::graph() call. Data is valid until the next execute::graph()
   //  call resets the counters.
   //
   //  All are inline — they compile to a single load instruction on C7x.
   // =========================================================================

   /**
    * @brief Get the number of nodes profiled in the last graph execution.
    *
    * Use this as the upper bound when looping over getNodeCycles().
    *
    * @return Number of nodes timed (0 to TISP_MAX_NODE_PROFILE_ENTRIES).
    */
   static inline uint32_t getNodeCount() { return nodeCount; }

   /**
    * @brief Get the cycle statistics for the node at position i.
    *
    * Position i corresponds to opVec[i] — the node at index i in the
    * signal chain vector. Use opVec[i]->getNodeName() to get the name
    * of the node that produced these cycle counts.
    *
    * The returned @c current field is the instantaneous measurement from the most
    * recent execution (the value this function used to return on its own), while
    * @c max, @c avg and @c count accumulate across every execution since the last
    * clear() or clearStats() call.
    *
    * @param i  Index in [0, getNodeCount()). Out-of-range yields an all-zero result.
    * @return @ref NodeCycleStats holding current, maximum, mean and execution count.
    */
   static inline NodeCycleStats getNodeCycles(uint32_t i)
   {
      NodeCycleStats stats = {0U, 0U, 0U, 0U};

      if (i < TISP_MAX_NODE_PROFILE_ENTRIES) {
         stats.current = nodeCycleCounts[i];
         stats.max     = nodeMaxCycles[i];
         stats.count   = nodeExecCount[i];
         stats.avg     = (stats.count > 0U) ? (nodeTotalCycles[i] / stats.count) : 0U;
      }
      else {
         /* Nothing to do here - leave the all-zero result */
      }

      return stats;
   }

   /**
    * @brief Get the cycles consumed by the most recent execution of node i.
    *
    * Scalar equivalent of getNodeCycles(i).current, for callers that want a
    * single value without materializing the statistics struct.
    *
    * @param i  Index in [0, getNodeCount()). Out-of-range returns 0.
    * @return CPU clock cycles consumed by node i's last exec() call.
    */
   static inline uint64_t getNodeLastCycles(uint32_t i)
   {
      return (i < TISP_MAX_NODE_PROFILE_ENTRIES) ? nodeCycleCounts[i] : 0U;
   }

   /**
    * @brief Get the largest cycle count observed for node i.
    *
    * @param i  Index in [0, getNodeCount()). Out-of-range returns 0.
    * @return Peak CPU clock cycles for node i since the last stats reset.
    */
   static inline uint64_t getNodeMaxCycles(uint32_t i)
   {
      return (i < TISP_MAX_NODE_PROFILE_ENTRIES) ? nodeMaxCycles[i] : 0U;
   }

   /**
    * @brief Get the cumulative cycle count for node i.
    *
    * @param i  Index in [0, getNodeCount()). Out-of-range returns 0.
    * @return Sum of every measurement for node i since the last stats reset.
    */
   static inline uint64_t getNodeTotalCycles(uint32_t i)
   {
      return (i < TISP_MAX_NODE_PROFILE_ENTRIES) ? nodeTotalCycles[i] : 0U;
   }

   /**
    * @brief Get the number of recorded executions of node i.
    *
    * @param i  Index in [0, getNodeCount()). Out-of-range returns 0.
    * @return Execution count for node i since the last stats reset.
    */
   static inline uint64_t getNodeExecCount(uint32_t i)
   {
      return (i < TISP_MAX_NODE_PROFILE_ENTRIES) ? nodeExecCount[i] : 0U;
   }

   /**
    * @brief Get the mean cycles per execution for node i.
    *
    * @param i  Index in [0, getNodeCount()). Out-of-range returns 0.
    * @return Total cycles divided by execution count, or 0 if never executed.
    */
   static inline uint64_t getNodeAvgCycles(uint32_t i)
   {
      uint64_t avgCycles = 0U;

      if ((i < TISP_MAX_NODE_PROFILE_ENTRIES) && (nodeExecCount[i] > 0U)) {
         avgCycles = nodeTotalCycles[i] / nodeExecCount[i];
      }
      else {
         /* Nothing to do here */
      }

      return avgCycles;
   }

   /**
    * @brief Get the number of supernodes profiled in the last graph execution.
    *
    * Use this as the upper bound when looping over getSuperNodeCycles().
    *
    * @return Number of supernodes timed (0 to TISP_MAX_SUPERNODE_PROFILE_ENTRIES).
    */
   static inline uint32_t getSuperNodeCount() { return superNodeCount; }

   /**
    * @brief Get CPU cycles consumed by the supernode at position i.
    *
    * This is the total wall-clock cycle cost of the entire supernode,
    * including DMA transfers and all inner node executions.
    *
    * @param i  Index in [0, getSuperNodeCount()). Out-of-range → undefined.
    * @return CPU clock cycles consumed by supernode i's exec() call.
    */
   static inline uint64_t getSuperNodeCycles(uint32_t i) { return superNodeCycleCounts[i]; }

   // =========================================================================
   //  END: Programmatic data access
   // =========================================================================

   /**
    * @brief Print all timing results with hierarchical view
    *
    * Prints a hierarchical view of all timing results to stdout,
    * showing supernodes and their contained nodes.
    */
   static void printResults();

   /**
    * @brief Print all timing results with hierarchical view, including node names
    *
    * Prints a hierarchical view of all timing results to stdout,
    * showing supernodes and their contained nodes with names from the provided opVec.
    *
    * @param myOpVec Vector of nodes with name information
    */
   static void printResults(const TISP::opVec& myOpVec);

   /**
    * @brief Print all timing results with hierarchical view, including supernode names
    *
    * Prints a hierarchical view of all timing results to stdout,
    * showing supernodes and their contained nodes with names from the provided superNodeVec.
    *
    * @param mySuperNodeVec Vector of supernodes with name information
    */
   static void printResults(const TISP::SuperNode::superNodeVec& mySuperNodeVec);

   /**
    * @brief Write profiling results to a JSON file
    *
    * Writes all profiling results to a JSON file, including
    * summary information, supernodes, and nodes.
    *
    * @param filename Name of the JSON file to write
    * @return true if successful, false otherwise
    */
   static bool writeJsonResults(const char* filename);

   /**
    * @brief Write profiling results to a JSON file, including node names
    *
    * Writes all profiling results to a JSON file, including
    * summary information, supernodes, and nodes with names from the provided opVec.
    *
    * @param filename Name of the JSON file to write
    * @param myOpVec Vector of nodes with name information
    * @return true if successful, false otherwise
    */
   static bool writeJsonResults(const char* filename, const TISP::opVec& myOpVec);

   /**
    * @brief Write profiling results to a JSON file, including supernode names
    *
    * Writes all profiling results to a JSON file, including
    * summary information, supernodes, and nodes with names from the provided superNodeVec.
    *
    * @param filename Name of the JSON file to write
    * @param mySuperNodeVec Vector of supernodes with name information
    * @return true if successful, false otherwise
    */
   static bool writeJsonResults(const char* filename, const TISP::SuperNode::superNodeVec& mySuperNodeVec);

   /**
    * @brief Print memory usage information
    *
    * Prints information about the memory usage of the NodeProfiler class,
    * including the size of node arrays, supernode arrays, and other data.
    */
   static void printMemoryUsage();
};

// Static member declarations - definitions are in TISP_profiler.cpp
