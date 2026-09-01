// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// Include the header file with proper path
#include "TISP_profiler.hpp"

// Initialize static members - Node tracking
uint64_t NodeProfiler::nodeStartCycles[TISP_MAX_NODE_PROFILE_ENTRIES] = {0}; ///< Start cycle timestamps for each node
uint64_t NodeProfiler::nodeCycleCounts[TISP_MAX_NODE_PROFILE_ENTRIES] = {0}; ///< Cycle counts for each node
uint32_t NodeProfiler::nodeIds[TISP_MAX_NODE_PROFILE_ENTRIES]         = {0}; ///< IDs for each node
uint32_t NodeProfiler::nodeParentSuperNodeId[TISP_MAX_NODE_PROFILE_ENTRIES] = {
    0};                               ///< Which supernode contains this node
uint32_t NodeProfiler::nodeCount = 0; ///< Current number of nodes being tracked

// Initialize static members - Node cycle statistics (accumulated across executions)
uint64_t NodeProfiler::nodeMaxCycles[TISP_MAX_NODE_PROFILE_ENTRIES]   = {0}; ///< Largest cycle count per node
uint64_t NodeProfiler::nodeTotalCycles[TISP_MAX_NODE_PROFILE_ENTRIES] = {0}; ///< Running sum of cycles per node
uint64_t NodeProfiler::nodeExecCount[TISP_MAX_NODE_PROFILE_ENTRIES]   = {0}; ///< Executions recorded per node

// Initialize static members - Supernode tracking
uint64_t NodeProfiler::superNodeStartCycles[TISP_MAX_SUPERNODE_PROFILE_ENTRIES] = {
    0}; ///< Start cycle timestamps for each supernode
uint64_t NodeProfiler::superNodeCycleCounts[TISP_MAX_SUPERNODE_PROFILE_ENTRIES] = {
    0};                                                                        ///< Cycle counts for each supernode
uint32_t NodeProfiler::superNodeIds[TISP_MAX_SUPERNODE_PROFILE_ENTRIES] = {0}; ///< IDs for each supernode
uint32_t NodeProfiler::superNodeCount = 0; ///< Current number of supernodes being tracked

// Initialize static members - Context tracking
uint32_t NodeProfiler::currentSuperNodeId = 0;     ///< ID of the currently executing supernode
bool     NodeProfiler::inSuperNode        = false; ///< Whether execution is currently inside a supernode

// Initialize static members - Common data
uint64_t NodeProfiler::overhead = 0; ///< Profiling overhead in cycles

/**
 * @brief Print all timing results with hierarchical view
 *
 * Prints a hierarchical view of all timing results to stdout,
 * showing supernodes and their contained nodes.
 */
void NodeProfiler::printResults()
{
   // Calculate node time (nodes not part of any supernode)
   uint64_t nodeTime = 0;
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         nodeTime += nodeCycleCounts[i];
      }
   }

   // Sum up all supernode cycle counts
   uint64_t totalSuperNodeTime = 0;
   for (uint32_t i = 0; i < superNodeCount; i++) {
      totalSuperNodeTime += superNodeCycleCounts[i];
   }

   // Total time is the sum of node time and supernode time
   uint64_t totalTime = nodeTime + totalSuperNodeTime;

   (void) printf("Profiling Results:\n");
   (void) printf("-----------------------\n");
#ifdef HOST_EMULATION
   (void) printf("NOTE: Running in HOST_EMULATION mode - cycle counts will be 0\n");
#endif
   (void) printf("Total Time: %lu cycles\n", totalTime);
   if (superNodeCount > 0) {
      (void) printf("SuperNode Time: %lu cycles\n", totalSuperNodeTime);
   }
   (void) printf("Node Time: %lu cycles\n", nodeTime);
   (void) printf("Nodes: %u/%u, SuperNodes: %u/%u\n", nodeCount, TISP_MAX_NODE_PROFILE_ENTRIES, superNodeCount,
                 TISP_MAX_SUPERNODE_PROFILE_ENTRIES);

   // Print supernodes with their contained nodes
   if (superNodeCount > 0) {
      for (uint32_t i = 0; i < superNodeCount; i++) {
         uint32_t superId = superNodeIds[i];
         (void) printf("SuperNode ID: %u, Cycles: %lu\n", superId, superNodeCycleCounts[i]);

         // Find and print all nodes that belong to this supernode
         for (uint32_t j = 0; j < nodeCount; j++) {
            if (nodeParentSuperNodeId[j] == superId) {
               (void) printf("  - Node ID: %u, Cycles: %lu, Max: %lu, Avg: %lu, Count: %lu\n", nodeIds[j],
                             nodeCycleCounts[j], nodeMaxCycles[j], getNodeAvgCycles(j), nodeExecCount[j]);
            }
         }
      }
   }

   // Print nodes (not part of any supernode)
   (void) printf("\nNodes:\n");
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         (void) printf("Node ID: %u, Cycles: %lu, Max: %lu, Avg: %lu, Count: %lu\n", nodeIds[i],
                       nodeCycleCounts[i], nodeMaxCycles[i], getNodeAvgCycles(i), nodeExecCount[i]);
      }
   }
   (void) printf("-----------------------\n");
}

/**
 * @brief Print all timing results with hierarchical view, including node names
 *
 * Prints a hierarchical view of all timing results to stdout,
 * showing supernodes and their contained nodes with names from the provided opVec.
 *
 * @param myOpVec Vector of nodes with name information
 */
void NodeProfiler::printResults(const TISP::opVec& myOpVec)
{
   // Calculate node time (nodes not part of any supernode)
   uint64_t nodeTime = 0;
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         nodeTime += nodeCycleCounts[i];
      }
   }

   // Sum up all supernode cycle counts
   uint64_t totalSuperNodeTime = 0;
   for (uint32_t i = 0; i < superNodeCount; i++) {
      totalSuperNodeTime += superNodeCycleCounts[i];
   }

   // Total time is the sum of node time and supernode time
   uint64_t totalTime = nodeTime + totalSuperNodeTime;

   (void) printf("Profiling Results:\n");
   (void) printf("-----------------------\n");
#ifdef HOST_EMULATION
   (void) printf("NOTE: Running in HOST_EMULATION mode - cycle counts will be 0\n");
#endif
   (void) printf("Total Time: %lu cycles\n", totalTime);
   if (superNodeCount > 0) {
      (void) printf("SuperNode Time: %lu cycles\n", totalSuperNodeTime);
   }
   (void) printf("Node Time: %lu cycles\n", nodeTime);
   (void) printf("Nodes: %u/%u, SuperNodes: %u/%u\n", nodeCount, TISP_MAX_NODE_PROFILE_ENTRIES, superNodeCount,
                 TISP_MAX_SUPERNODE_PROFILE_ENTRIES);

   // Print supernodes with their contained nodes
   if (superNodeCount > 0) {
      for (uint32_t i = 0; i < superNodeCount; i++) {
         uint32_t superId = superNodeIds[i];
         (void) printf("SuperNode ID: %u, Cycles: %lu\n", superId, superNodeCycleCounts[i]);

         // Find and print all nodes that belong to this supernode
         for (uint32_t j = 0; j < nodeCount && j < myOpVec.size(); j++) {
            if (nodeParentSuperNodeId[j] == superId) {
               (void) printf("  └─ Node ID: %u, Name: %s, Cycles: %lu, Max: %lu, Avg: %lu, Count: %lu\n",
                             nodeIds[j], myOpVec[j]->getNodeName(), nodeCycleCounts[j], nodeMaxCycles[j],
                             getNodeAvgCycles(j), nodeExecCount[j]);
            }
         }
      }
   }

   // Print nodes (not part of any supernode)
   (void) printf("\nNodes:\n");
   for (uint32_t i = 0; i < nodeCount && i < myOpVec.size(); i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         (void) printf("Node ID: %u, Name: %s, Cycles: %lu, Max: %lu, Avg: %lu, Count: %lu\n", nodeIds[i],
                       myOpVec[i]->getNodeName(), nodeCycleCounts[i], nodeMaxCycles[i], getNodeAvgCycles(i),
                       nodeExecCount[i]);
      }
   }
   (void) printf("-----------------------\n");
}

/**
 * @brief Print all timing results with hierarchical view, including supernode names
 *
 * Prints a hierarchical view of all timing results to stdout,
 * showing supernodes and their contained nodes with names from the provided superNodeVec.
 *
 * @param mySuperNodeVec Vector of supernodes with name information
 */
void NodeProfiler::printResults(const TISP::SuperNode::superNodeVec& mySuperNodeVec)
{
   // Calculate node time (nodes not part of any supernode)
   uint64_t nodeTime = 0;
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         nodeTime += nodeCycleCounts[i];
      }
   }

   // Sum up all supernode cycle counts
   uint64_t totalSuperNodeTime = 0;
   for (uint32_t i = 0; i < superNodeCount; i++) {
      totalSuperNodeTime += superNodeCycleCounts[i];
   }

   // Total time is the sum of node time and supernode time
   uint64_t totalTime = nodeTime + totalSuperNodeTime;

   (void) printf("Profiling Results:\n");
   (void) printf("-----------------------\n");
#ifdef HOST_EMULATION
   (void) printf("NOTE: Running in HOST_EMULATION mode - cycle counts will be 0\n");
#endif
   (void) printf("Total Time: %lu cycles\n", totalTime);
   if (superNodeCount > 0) {
      (void) printf("SuperNode Time: %lu cycles\n", totalSuperNodeTime);
   }
   (void) printf("Node Time: %lu cycles\n", nodeTime);
   (void) printf("Nodes: %u/%u, SuperNodes: %u/%u\n", nodeCount, TISP_MAX_NODE_PROFILE_ENTRIES, superNodeCount,
                 TISP_MAX_SUPERNODE_PROFILE_ENTRIES);

   // Print supernodes with their contained nodes
   if (superNodeCount > 0) {
      for (uint32_t i = 0; i < superNodeCount && i < mySuperNodeVec.size(); i++) {
         uint32_t superId = superNodeIds[i];
         (void) printf("SuperNode ID: %u, Cycles: %lu\n", superId, superNodeCycleCounts[i]);

         // Find and print all nodes that belong to this supernode
         for (uint32_t j = 0; j < nodeCount; j++) {
            if (nodeParentSuperNodeId[j] == superId) {
               (void) printf("  └─ Node ID: %u, Cycles: %lu, Max: %lu, Avg: %lu, Count: %lu\n", nodeIds[j],
                             nodeCycleCounts[j], nodeMaxCycles[j], getNodeAvgCycles(j), nodeExecCount[j]);
            }
         }
      }
   }

   // Print nodes (not part of any supernode)
   (void) printf("\nNodes:\n");
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         (void) printf("Node ID: %u, Cycles: %lu, Max: %lu, Avg: %lu, Count: %lu\n", nodeIds[i],
                       nodeCycleCounts[i], nodeMaxCycles[i], getNodeAvgCycles(i), nodeExecCount[i]);
      }
   }
   (void) printf("-----------------------\n");
}

/**
 * @brief Write profiling results to a JSON file
 *
 * Writes all profiling results to a JSON file, including
 * summary information, supernodes, and nodes.
 *
 * @param filename Name of the JSON file to write
 * @return true if successful, false otherwise
 */
bool NodeProfiler::writeJsonResults(const char* filename)
{
   // Open the file for writing
   FILE* jsonFile = fopen(filename, "w");
   if (jsonFile == nullptr) {
      (void) printf("Error: Could not open file %s for writing\n", filename);
      return false;
   }

   // Calculate node time (nodes not part of any supernode)
   uint64_t nodeTime = 0;
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         nodeTime += nodeCycleCounts[i];
      }
   }

   // Sum up all supernode cycle counts
   uint64_t totalSuperNodeTime = 0;
   for (uint32_t i = 0; i < superNodeCount; i++) {
      totalSuperNodeTime += superNodeCycleCounts[i];
   }

   // Total time is the sum of node time and supernode time
   uint64_t totalTime = nodeTime + totalSuperNodeTime;

   // Start writing JSON
   (void) fprintf(jsonFile, "{\n");

   // Write summary information
   (void) fprintf(jsonFile, "  \"summary\": {\n");
#ifdef HOST_EMULATION
   (void) fprintf(jsonFile, "    \"host_emulation\": true,\n");
#else
   (void) fprintf(jsonFile, "    \"host_emulation\": false,\n");
#endif
   (void) fprintf(jsonFile, "    \"total_time_cycles\": %lu,\n", totalTime);
   (void) fprintf(jsonFile, "    \"supernode_time_cycles\": %lu,\n", totalSuperNodeTime);
   (void) fprintf(jsonFile, "    \"node_time_cycles\": %lu,\n", nodeTime);
   (void) fprintf(jsonFile, "    \"node_count\": %u,\n", nodeCount);
   (void) fprintf(jsonFile, "    \"max_nodes\": %u,\n", TISP_MAX_NODE_PROFILE_ENTRIES);
   (void) fprintf(jsonFile, "    \"supernode_count\": %u,\n", superNodeCount);
   (void) fprintf(jsonFile, "    \"max_supernodes\": %u\n", TISP_MAX_SUPERNODE_PROFILE_ENTRIES);
   (void) fprintf(jsonFile, "  },\n");

   // Write supernodes with their contained nodes
   (void) fprintf(jsonFile, "  \"supernodes\": [\n");
   for (uint32_t i = 0; i < superNodeCount; i++) {
      uint32_t superId = superNodeIds[i];

      (void) fprintf(jsonFile, "    {\n");
      (void) fprintf(jsonFile, "      \"id\": %u,\n", superId);
      (void) fprintf(jsonFile, "      \"cycles\": %lu,\n", superNodeCycleCounts[i]);
      (void) fprintf(jsonFile, "      \"num_blocks\": 0,\n"); // Default number of blocks

      // Find and write all nodes that belong to this supernode
      (void) fprintf(jsonFile, "      \"nodes\": [\n");
      bool firstNode = true;
      for (uint32_t j = 0; j < nodeCount; j++) {
         if (nodeParentSuperNodeId[j] == superId) {
            if (!firstNode) {
               (void) fprintf(jsonFile, ",\n");
            }
            (void) fprintf(jsonFile, "        {\n");
            (void) fprintf(jsonFile, "          \"id\": %u,\n", nodeIds[j]);
            (void) fprintf(jsonFile, "          \"cycles\": %lu\n", nodeCycleCounts[j]);
            (void) fprintf(jsonFile, "        }");
            firstNode = false;
         }
      }
      (void) fprintf(jsonFile, "\n      ]\n");

      if (i < superNodeCount - 1) {
         (void) fprintf(jsonFile, "    },\n");
      }
      else {
         (void) fprintf(jsonFile, "    }\n");
      }
   }
   (void) fprintf(jsonFile, "  ],\n");

   // Write nodes (not part of any supernode)
   (void) fprintf(jsonFile, "  \"nodes\": [\n");
   bool firstNode = true;
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         if (!firstNode) {
            (void) fprintf(jsonFile, ",\n");
         }
         (void) fprintf(jsonFile, "    {\n");
         (void) fprintf(jsonFile, "      \"id\": %u,\n", nodeIds[i]);
         (void) fprintf(jsonFile, "      \"cycles\": %lu\n", nodeCycleCounts[i]);
         (void) fprintf(jsonFile, "    }");
         firstNode = false;
      }
   }
   (void) fprintf(jsonFile, "\n  ]\n");

   // Close the JSON object
   (void) fprintf(jsonFile, "}\n");

   // Close the file
   (void) fclose(jsonFile);

   (void) printf("Profiling results written to %s\n", filename);
   return true;
}

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
bool NodeProfiler::writeJsonResults(const char* filename, const TISP::opVec& myOpVec)
{
   // Open the file for writing
   FILE* jsonFile = fopen(filename, "w");
   if (jsonFile == nullptr) {
      (void) printf("Error: Could not open file %s for writing\n", filename);
      return false;
   }

   // Calculate node time (nodes not part of any supernode)
   uint64_t nodeTime = 0;
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         nodeTime += nodeCycleCounts[i];
      }
   }

   // When using opVec, we don't have supernodes
   uint64_t totalSuperNodeTime = 0;
   uint64_t totalTime          = nodeTime;

   // Start writing JSON
   (void) fprintf(jsonFile, "{\n");

   // Write summary information
   (void) fprintf(jsonFile, "  \"summary\": {\n");
#ifdef HOST_EMULATION
   (void) fprintf(jsonFile, "    \"host_emulation\": true,\n");
#else
   (void) fprintf(jsonFile, "    \"host_emulation\": false,\n");
#endif
   (void) fprintf(jsonFile, "    \"total_time_cycles\": %lu,\n", totalTime);
   (void) fprintf(jsonFile, "    \"supernode_time_cycles\": %lu,\n", totalSuperNodeTime);
   (void) fprintf(jsonFile, "    \"node_time_cycles\": %lu,\n", nodeTime);
   (void) fprintf(jsonFile, "    \"node_count\": %u,\n", nodeCount);
   (void) fprintf(jsonFile, "    \"max_nodes\": %u,\n", TISP_MAX_NODE_PROFILE_ENTRIES);
   (void) fprintf(jsonFile, "    \"supernode_count\": 0,\n");
   (void) fprintf(jsonFile, "    \"max_supernodes\": %u\n", TISP_MAX_SUPERNODE_PROFILE_ENTRIES);
   (void) fprintf(jsonFile, "  },\n");

   // Write empty supernodes array (not applicable for opVec)
   (void) fprintf(jsonFile, "  \"supernodes\": [],\n");

   // Write nodes
   (void) fprintf(jsonFile, "  \"nodes\": [\n");
   bool firstNode = true;
   for (uint32_t i = 0; i < nodeCount && i < myOpVec.size(); i++) {
      if (!firstNode) {
         (void) fprintf(jsonFile, ",\n");
      }
      (void) fprintf(jsonFile, "    {\n");
      (void) fprintf(jsonFile, "      \"id\": %u,\n", nodeIds[i]);
      (void) fprintf(jsonFile, "      \"name\": \"%s\",\n", myOpVec[i]->getNodeName());
      (void) fprintf(jsonFile, "      \"node_id\": %u,\n", myOpVec[i]->getNodeId());
      (void) fprintf(jsonFile, "      \"params\": \"%s\",\n", myOpVec[i]->getNodeParams());
      (void) fprintf(jsonFile, "      \"cycles\": %lu\n", nodeCycleCounts[i]);
      (void) fprintf(jsonFile, "    }");
      firstNode = false;
   }
   (void) fprintf(jsonFile, "\n  ]\n");

   // Close the JSON object
   (void) fprintf(jsonFile, "}\n");

   // Close the file
   (void) fclose(jsonFile);

   (void) printf("Profiling results written to %s\n", filename);
   return true;
}

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
bool NodeProfiler::writeJsonResults(const char* filename, const TISP::SuperNode::superNodeVec& mySuperNodeVec)
{
   // Open the file for writing
   FILE* jsonFile = fopen(filename, "w");
   if (jsonFile == nullptr) {
      (void) printf("Error: Could not open file %s for writing\n", filename);
      return false;
   }

   // Calculate node time (nodes not part of any supernode)
   uint64_t nodeTime = 0;
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         nodeTime += nodeCycleCounts[i];
      }
   }

   // Sum up all supernode cycle counts
   uint64_t totalSuperNodeTime = 0;
   for (uint32_t i = 0; i < superNodeCount; i++) {
      totalSuperNodeTime += superNodeCycleCounts[i];
   }

   // Total time is the sum of node time and supernode time
   uint64_t totalTime = nodeTime + totalSuperNodeTime;

   // Start writing JSON
   (void) fprintf(jsonFile, "{\n");

   // Write summary information
   (void) fprintf(jsonFile, "  \"summary\": {\n");
#ifdef HOST_EMULATION
   (void) fprintf(jsonFile, "    \"host_emulation\": true,\n");
#else
   (void) fprintf(jsonFile, "    \"host_emulation\": false,\n");
#endif
   (void) fprintf(jsonFile, "    \"total_time_cycles\": %lu,\n", totalTime);
   (void) fprintf(jsonFile, "    \"supernode_time_cycles\": %lu,\n", totalSuperNodeTime);
   (void) fprintf(jsonFile, "    \"node_time_cycles\": %lu,\n", nodeTime);
   (void) fprintf(jsonFile, "    \"node_count\": %u,\n", nodeCount);
   (void) fprintf(jsonFile, "    \"max_nodes\": %u,\n", TISP_MAX_NODE_PROFILE_ENTRIES);
   (void) fprintf(jsonFile, "    \"supernode_count\": %u,\n", superNodeCount);
   (void) fprintf(jsonFile, "    \"max_supernodes\": %u\n", TISP_MAX_SUPERNODE_PROFILE_ENTRIES);
   (void) fprintf(jsonFile, "  },\n");

   // Write supernodes with their contained nodes
   (void) fprintf(jsonFile, "  \"supernodes\": [\n");
   for (uint32_t i = 0; i < superNodeCount && i < mySuperNodeVec.size(); i++) {
      uint32_t superId   = superNodeIds[i];
      auto&    superNode = mySuperNodeVec[i];
      auto&    opVec     = superNode->getOpVec();

      (void) fprintf(jsonFile, "    {\n");
      (void) fprintf(jsonFile, "      \"id\": %u,\n", superId);
      (void) fprintf(jsonFile, "      \"cycles\": %lu,\n", superNodeCycleCounts[i]);

      // Get number of blocks from the supernode
      size_t numBlocks = superNode->getBlockCount();
      (void) fprintf(jsonFile, "      \"num_blocks\": %zu,\n", numBlocks);

      // Find and write all nodes that belong to this supernode
      (void) fprintf(jsonFile, "      \"nodes\": [\n");
      bool     firstNode  = true;
      uint32_t opVecIndex = 0;
      for (uint32_t j = 0; j < nodeCount; j++) {
         if (nodeParentSuperNodeId[j] == superId) {
            if (!firstNode) {
               (void) fprintf(jsonFile, ",\n");
            }
            (void) fprintf(jsonFile, "        {\n");
            (void) fprintf(jsonFile, "          \"id\": %u,\n", nodeIds[j]);

            // Include node name, node_id, and params if available in the opVec
            if (opVecIndex < opVec.size()) {
               (void) fprintf(jsonFile, "          \"name\": \"%s\",\n", opVec[opVecIndex]->getNodeName());
               (void) fprintf(jsonFile, "          \"node_id\": %u,\n", opVec[opVecIndex]->getNodeId());
               (void) fprintf(jsonFile, "          \"params\": \"%s\",\n", opVec[opVecIndex]->getNodeParams());
               opVecIndex++;
            }

            (void) fprintf(jsonFile, "          \"cycles\": %lu\n", nodeCycleCounts[j]);
            (void) fprintf(jsonFile, "        }");
            firstNode = false;
         }
      }
      (void) fprintf(jsonFile, "\n      ]\n");

      if (i < superNodeCount - 1) {
         (void) fprintf(jsonFile, "    },\n");
      }
      else {
         (void) fprintf(jsonFile, "    }\n");
      }
   }
   (void) fprintf(jsonFile, "  ],\n");

   // Write nodes (not part of any supernode)
   (void) fprintf(jsonFile, "  \"nodes\": [\n");
   bool firstIndependentNode = true;
   for (uint32_t i = 0; i < nodeCount; i++) {
      if (nodeParentSuperNodeId[i] == 0) {
         if (!firstIndependentNode) {
            (void) fprintf(jsonFile, ",\n");
         }
         (void) fprintf(jsonFile, "    {\n");
         (void) fprintf(jsonFile, "      \"id\": %u,\n", nodeIds[i]);
         (void) fprintf(jsonFile, "      \"cycles\": %lu\n", nodeCycleCounts[i]);
         (void) fprintf(jsonFile, "    }");
         firstIndependentNode = false;
      }
   }
   (void) fprintf(jsonFile, "\n  ]\n");

   // Close the JSON object
   (void) fprintf(jsonFile, "}\n");

   // Close the file
   (void) fclose(jsonFile);

   (void) printf("Profiling results written to %s\n", filename);
   return true;
}

/**
 * @brief Print memory usage information
 *
 * Prints information about the memory usage of the NodeProfiler class,
 * including the size of node arrays, supernode arrays, and other data.
 */
void NodeProfiler::printMemoryUsage()
{
   size_t nodeMemory = sizeof(nodeStartCycles) + sizeof(nodeCycleCounts) + sizeof(nodeIds) +
                       sizeof(nodeParentSuperNodeId) + sizeof(nodeMaxCycles) + sizeof(nodeTotalCycles) +
                       sizeof(nodeExecCount);
   size_t superNodeMemory = sizeof(superNodeStartCycles) + sizeof(superNodeCycleCounts) + sizeof(superNodeIds);
   size_t otherMemory =
       sizeof(overhead) + sizeof(nodeCount) + sizeof(superNodeCount) + sizeof(currentSuperNodeId) + sizeof(inSuperNode);
   size_t totalMemory = nodeMemory + superNodeMemory + otherMemory;

   (void) printf("NodeProfiler Memory Usage:\n");
   (void) printf("-----------------------\n");
   (void) printf("Node arrays: %zu bytes\n", nodeMemory);
   (void) printf("SuperNode arrays: %zu bytes\n", superNodeMemory);
   (void) printf("Other data: %zu bytes\n", otherMemory);
   (void) printf("Total: %zu bytes\n", totalMemory);
   (void) printf("-----------------------\n");
}
