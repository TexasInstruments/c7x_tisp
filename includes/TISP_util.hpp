// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_dataProbe.hpp"  // Node-boundary data probe
#include "TISP_profiler.hpp" // Include our new profiler
#include "TISP_superNode.hpp"
#include "TISP_virtualOp.hpp"
#include <TISP_types.hpp>
#include <cstdint>
#include <cstdlib>
#include <list>
#include <vector>
#include <cstdio>

namespace TISP {

/**
   \class execute
   @brief Provides the execute function of the graph
 */

class execute {
 public:
   /**
    *  @ingroup  NODE
    */
   /**@{*/
   /**
    *  @brief This function takes in alias of the opeVec (list of nodes) and iterates through all the nodes calling the
    *         exec function pertaining to that node
    *  @param [in] myOpVec  : Object to list of nodes
    *
    */
   static void graph(const opVec &myOpVec)
   {
      // Reset profiler counters before executing the graph if not inside a supernode
#pragma FORCEINLINE
      if (!NodeProfiler::isInSuperNode()) {
#pragma FORCEINLINE
         NodeProfiler::resetCounters();
         // Data probe: count the pass and arm.
#pragma FORCEINLINE
         DataProbe::onFrameStart();
      }
      for (auto const &op : myOpVec) {
#pragma FORCEINLINE
         DataProbe::onNodeBoundary(*op, NodeExecPhase::kNodeExecPre);
#pragma FORCEINLINE
         NodeProfiler::startNode();
         op->exec();
#pragma FORCEINLINE
         NodeProfiler::stopNode();
#pragma FORCEINLINE
         DataProbe::onNodeBoundary(*op, NodeExecPhase::kNodeExecPost);
      }

      return;
   }

   static void graph(std::unique_ptr<opVec> const &myOpVec)
   {
      // Reset profiler counters before executing the graph if not inside a supernode
#pragma FORCEINLINE
      if (!NodeProfiler::isInSuperNode()) {
#pragma FORCEINLINE
         NodeProfiler::resetCounters();
         // Data probe: count the pass and arm.
#pragma FORCEINLINE
         DataProbe::onFrameStart();
      }
      for (auto const &op : *myOpVec) {
#pragma FORCEINLINE
         DataProbe::onNodeBoundary(*op, NodeExecPhase::kNodeExecPre);
#pragma FORCEINLINE
         NodeProfiler::startNode();
         op->exec();
#pragma FORCEINLINE
         NodeProfiler::stopNode();
#pragma FORCEINLINE
         DataProbe::onNodeBoundary(*op, NodeExecPhase::kNodeExecPost);
      }

      return;
   }

   static void graph(opVec *myOpVec)
   {
      // Reset profiler counters before executing the graph if not inside a supernode
#pragma FORCEINLINE
      if (!NodeProfiler::isInSuperNode()) {
#pragma FORCEINLINE
         NodeProfiler::resetCounters();
         // Data probe: count the pass and arm.
#pragma FORCEINLINE
         DataProbe::onFrameStart();
      }
      for (auto const &op : *myOpVec) {
#pragma FORCEINLINE
         DataProbe::onNodeBoundary(*op, NodeExecPhase::kNodeExecPre);
#pragma FORCEINLINE
         NodeProfiler::startNode();
         op->exec();
#pragma FORCEINLINE
         NodeProfiler::stopNode();
#pragma FORCEINLINE
         DataProbe::onNodeBoundary(*op, NodeExecPhase::kNodeExecPost);
      }

      return;
   }

   /** @} */

   /**
    *  @ingroup  SUPERNODE
    */

   /**@{*/

   /**
    *  @brief This function takes in alias of the superNodeVec (list of supernodes) and iterates through all the
    *         supernodes calling the exec function pertaining to that supernode
    *  @param [in] mySuperNodeVec  : Object to list of supernodes
    *
    */
   static void graph(const SuperNode::superNodeVec &mySuperNodeVec)
   {
      // Reset profiler counters before executing the graph
#pragma FORCEINLINE_RECURSIVE
      NodeProfiler::resetCounters();
      for (auto const &op : mySuperNodeVec) {
#pragma FORCEINLINE_RECURSIVE
         NodeProfiler::startSuperNode();
         op->exec();
#pragma FORCEINLINE_RECURSIVE
         NodeProfiler::stopSuperNode();
      }

      return;
   }

   static void graph(SuperNode::superNodeVec *mySuperNodeVec)
   {
      // Reset profiler counters before executing the graph
#pragma FORCEINLINE_RECURSIVE
      NodeProfiler::resetCounters();
      for (auto const &op : *mySuperNodeVec) {
#pragma FORCEINLINE_RECURSIVE
         NodeProfiler::startSuperNode();
         op->exec();
#pragma FORCEINLINE_RECURSIVE
         NodeProfiler::stopSuperNode();
      }

      return;
   }

   /** @} */
};

/**
 * @brief Print L2SRAM arena usage for all three TISP arenas.
 *
 * Call after graph construction to report bytes consumed by each arena.
 */
inline void printArenaStats()
{
   auto report = [](const char *name, BumpAllocator &a) {
      size_t used    = a.getBytesUsed();
      size_t total   = a.getBufferSize();
      size_t percent = (total > 0u) ? ((used * 100u) / total) : 0u;
      (void) printf("  %-20s : %5zu / %5zu bytes  (%3zu%%)\n",
                    name, used, total, percent);
   };

   (void) printf("=== TISP Arena Usage ===\n");
   report("node arena",    nodeAllocator());
   report("twiddle arena", twiddleAllocator());
   report("handle arena",  handleAllocator());
   (void) printf("========================\n");
}

} // namespace TISP
