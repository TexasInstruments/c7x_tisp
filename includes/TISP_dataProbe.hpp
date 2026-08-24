// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_types.hpp"
#include "TISP_virtualOp.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace TISP {

/**
   @brief Node boundary at which a dataProbe is taken.
 */
enum class NodeExecPhase : uint8_t {
   kNodeExecPre  = 0U, /**< Immediately before the node's exec(). */
   kNodeExecPost = 1U  /**< Immediately after the node's exec(). */
};

/**
   @brief Which side of a node the dataProbe reads.
 */
enum class DataProbePortType : uint8_t {
   kDataProbePortInput  = 0U, /**< Read one of the node's input buffers. */
   kDataProbePortOutput = 1U  /**< Read one of the node's output buffers. */
};

/**
   @brief Lifecycle of the single dataProbe session.

   @details The application drives every transition except
            @ref DataProbeState::kDataProbeReady, which the graph loop performs when
            it has filled the destination buffer:

            @verbatim
            off --start()--> idle --arm()--> armed --(graph copies)--> ready
             ^                 ^                                        |
             |                 +----------------- release() ------------+
             +--------------------------- stop() -----------------------
            @endverbatim

            @ref DataProbe::start is only accepted from
            @ref DataProbeState::kDataProbeOff, so a session that is armed, or that
            holds a frame the application has not consumed yet, is never
            discarded behind its back.
 */
enum class DataProbeState : uint8_t {
   kDataProbeOff   = 0U, /**< No session: the graph loop does nothing. */
   kDataProbeIdle  = 1U, /**< Session configured, no frame requested. */
   kDataProbeArmed = 2U, /**< One frame requested; the next matching node fills it. */
   kDataProbeReady = 3U  /**< Destination buffer holds a frame, waiting to be consumed. */
};

/**
   @brief Resolved description of one dataProbe session.

   @details The geometry fields are resolved once, in @ref DataProbe::start, from
            the node's @ref NodeBufParams. Buffer geometry is fixed after graph
            construction, so only the port address has to be re-read at each
            node boundary (the first and last nodes are re-pointed at the
            McASP ping/pong buffers every frame).
 */
struct DataProbeSession {
   uint32_t          nodeId{0U};                                       /**< Node whose port is probed. */
   DataProbePortType portType{DataProbePortType::kDataProbePortInput}; /**< Side of the node that is read. */
   size_t            portIndex{0U};                                    /**< Port index within that side of the node. */
   NodeExecPhase     phase{NodeExecPhase::kNodeExecPre};                /**< Boundary at which the port is valid. */
   uint32_t          channel{0U};                                      /**< Channel to extract from the port. */
   uint8_t          *dest{nullptr};                                    /**< Application-owned destination buffer. */
   size_t            destBytes{0U};                                    /**< Capacity of @c dest, in bytes. */
   size_t            numSamples{0U};                                   /**< Samples in one channel of the port. */
   size_t            elementSize{0U};                                  /**< Bytes per sample. */
   size_t            pitchBytes{0U};                                   /**< Row stride of the port, in bytes. */
   size_t            frameBytes{0U};                                   /**< Bytes one dataProbe writes to @c dest. */
   bool              isInterleave{false};                              /**< Port layout: interleaved or planar. */
   volatile DataProbeState state{DataProbeState::kDataProbeOff};       /**< Session lifecycle. */
   volatile size_t         bytesProbed{0U};                            /**< Bytes written by the last dataProbe. */
};

/**
   \class DataProbe
   @brief Graph-side data probe: copies one channel of one node port into a
          buffer the application owns.

   @details A probe has to read a node's port while that buffer still holds the
            node's own data. Inter-operator buffers are packed by liveness, so a
            buffer is recycled as soon as its last consumer has run: anything
            that reads it after the graph completes sees whichever later node
            last wrote that memory, not the node it asked for. The copy
            therefore happens inside the graph loop, at the node boundary.

            The split of responsibilities is deliberate:
            - TISP owns the copy: it knows the port address, layout and pitch.
            - The application owns the destination buffer, the dataProbe cadence
              (@ref arm), and the transport of the probed frame.

            One frame is copied per @ref arm, so a graph running with no armed
            request costs one load and one compare per node boundary. All state
            is header-only: one session exists per executable.

   @note Not reentrant. @ref start / @ref stop / @ref arm / @ref release run in
         the application's context; @ref onNodeBoundary runs in the audio frame.
         Both touch @c state, which is a single byte written by only one of the
         two sides in any given state, so no lock is needed.
 */
class DataProbe {
 public:
   /**
    *  @ingroup  DATA_PROBE
    */
   /**@{*/

   /**
    *  @brief Configure a dataProbe session against a node port of @c myOpVec.
    *
    *  @details Validates the request and resolves the port geometry up front so
    *           that the per-frame path stays a bounded copy. No data is copied
    *           until the application calls @ref arm.
    *
    *  @note Only accepted while no session is configured. Call @ref stop first
    *        to reconfigure, so that an armed request or a frame the application
    *        has not consumed yet is never discarded silently.
    *
    *  @param [in] &myOpVec    : Graph to probe.
    *  @param [in] nodeId      : Node ID to probe, as reported by getNodeId().
    *  @param [in] portType    : Side of the node to read.
    *  @param [in] portIndex   : Port index within that side of the node.
    *  @param [in] channel     : Channel to extract from the port.
    *  @param [in] dest        : Destination buffer, owned by the caller. Must
    *                            stay valid until @ref stop.
    *  @param [in] destBytes   : Capacity of @c dest, in bytes.
    *  @retval ErrorCode::kNone on success, ErrorCode::kDataProbeBusy if a session is
    *          already configured, otherwise the reason the request was
    *          rejected. A rejected request leaves the current session, if any,
    *          untouched.
    */
   static ErrorCode start(const opVec       &myOpVec,
                          uint32_t           nodeId,
                          DataProbePortType  portType,
                          size_t             portIndex,
                          uint32_t           channel,
                          void              *dest,
                          size_t             destBytes) noexcept
   {
      DataProbeSession &s = session();

      ErrorCode status = ErrorCode::kNone;

      if (s.state != DataProbeState::kDataProbeOff) {
         status = ErrorCode::kDataProbeBusy;
      }

      if ((status == ErrorCode::kNone) && ((dest == nullptr) || (destBytes == 0U))) {
         status = ErrorCode::kNullPointer;
      }

      if ((status == ErrorCode::kNone) && (portType != DataProbePortType::kDataProbePortInput)
          && (portType != DataProbePortType::kDataProbePortOutput)) {
         status = ErrorCode::kInvalidParameter;
      }

      const bool isInput = (portType == DataProbePortType::kDataProbePortInput);

      const VirtualOp *op = nullptr;

      if (status == ErrorCode::kNone) {
         for (size_t i = 0U; i < myOpVec.size(); ++i) {
            if (myOpVec[i]->getNodeId() == nodeId) {
               op = myOpVec[i].get();
               break;
            }
         }
         if (op == nullptr) {
            status = ErrorCode::kInvalidObjectId;
         }
      }

      size_t numChannels = 0U;
      size_t numSamples  = 0U;
      size_t pitchBytes  = 0U;
      size_t elementSize = 0U;
      bool   interleave  = false;

      if (status == ErrorCode::kNone) {
         const NodeBufParams &bufParams = op->getNodeBufParams();
         const size_t         numPorts  = isInput ? op->getNumInputs() : op->getNumOutputs();
         // The geometry arrays below are fixed size, so the index has to be
         // inside the array bound as well as inside the count the node reports.
         const size_t maxPorts = isInput ? static_cast<size_t>(kMaxSrcAddr) : static_cast<size_t>(kMaxDstAddr);

         if ((portIndex >= numPorts) || (portIndex >= maxPorts)) {
            status = ErrorCode::kInvalidParameter;
         }
         else {
            if (isInput) {
               elementSize = static_cast<size_t>(sizeOf(bufParams.inputDataType));
               pitchBytes  = bufParams.inPitchBytes[portIndex];
               interleave  = (bufParams.isInputInterleave != 0U);
               numChannels = interleave ? bufParams.widthIn[portIndex] : bufParams.heightIn[portIndex];
               numSamples  = interleave ? bufParams.heightIn[portIndex] : bufParams.widthIn[portIndex];
            }
            else {
               elementSize = static_cast<size_t>(sizeOf(bufParams.outputDataType));
               pitchBytes  = bufParams.outPitchBytes[portIndex];
               interleave  = (bufParams.isOutputInterleave != 0U);
               numChannels = interleave ? bufParams.widthOut[portIndex] : bufParams.heightOut[portIndex];
               numSamples  = interleave ? bufParams.heightOut[portIndex] : bufParams.widthOut[portIndex];
            }

            if ((channel >= numChannels) || (numSamples == 0U) || (elementSize == 0U)) {
               status = ErrorCode::kInvalidParameter;
            }
            else if ((numSamples * elementSize) > destBytes) {
               status = ErrorCode::kMemory;
            }
            else {
               // Request accepted.
            }
         }
      }

      if (status == ErrorCode::kNone) {
         s.nodeId        = nodeId;
         s.portType      = portType;
         s.portIndex     = portIndex;
         // An input port is only intact before the node runs, since a kernel may
         // write its output over its input; an output port only after.
         s.phase         = isInput ? NodeExecPhase::kNodeExecPre : NodeExecPhase::kNodeExecPost;
         s.channel       = channel;
         s.dest          = static_cast<uint8_t *>(dest);
         s.destBytes     = destBytes;
         s.numSamples    = numSamples;
         s.elementSize   = elementSize;
         s.pitchBytes    = pitchBytes;
         s.frameBytes    = numSamples * elementSize;
         s.isInterleave  = interleave;
         s.bytesProbed   = 0U;
         s.state         = DataProbeState::kDataProbeIdle;
      }

      return status;
   }

   /**
    *  @brief Tear the dataProbe session down. The graph loop stops copying and the
    *         destination buffer may be reused by the caller.
    *  @return None.
    */
   static void stop() noexcept
   {
      DataProbeSession &s = session();
      s.state         = DataProbeState::kDataProbeOff;
      s.dest          = nullptr;
      s.bytesProbed   = 0U;
   }

   /**
    *  @brief Request one frame: the next matching node boundary fills the
    *         destination buffer.
    *
    *  @details Call this when the application's dataProbe interval elapses. It is
    *           ignored unless the session is idle, so a frame that has not been
    *           consumed yet is never overwritten.
    *  @retval true if the request was accepted.
    */
   static bool arm() noexcept
   {
      DataProbeSession &s = session();
      bool              accepted = (s.state == DataProbeState::kDataProbeIdle);
      if (accepted) {
         s.state = DataProbeState::kDataProbeArmed;
      }
      return accepted;
   }

   /**
    *  @brief Test whether the destination buffer holds a probed frame.
    *  @retval true if a frame is waiting to be consumed.
    */
   static bool isReady() noexcept { return (session().state == DataProbeState::kDataProbeReady); }

   /**
    *  @brief Bytes written to the destination buffer by the last dataProbe.
    *  @retval Byte count, valid while @ref isReady returns true.
    */
   static size_t bytesProbed() noexcept { return session().bytesProbed; }

   /**
    *  @brief Release the probed frame once the application has consumed it,
    *         so the next @ref arm can be honoured.
    *  @return None.
    */
   static void release() noexcept
   {
      DataProbeSession &s = session();
      if (s.state == DataProbeState::kDataProbeReady) {
         s.state = DataProbeState::kDataProbeIdle;
      }
   }

   /**
    *  @brief Test whether a dataProbe session is configured.
    *  @retval true between a successful @ref start and the matching @ref stop.
    */
   static bool isRunning() noexcept { return (session().state != DataProbeState::kDataProbeOff); }

   /** @} */

   /**
    *  @brief Node boundary hook, called by @ref execute::graph for every node.
    *
    *  @details Runs in the caller's context, inside the audio frame, so it does
    *           the minimum amount of work: bail out unless a frame is armed for
    *           this node at this phase, then a single channel copy. It is called
    *           outside the profiler start/stop pair so that it does not pollute
    *           the per-node cycle counts.
    *
    *  @param [in] &op    : Node that is about to run, or has just run.
    *  @param [in] phase  : Boundary this call represents.
    *  @return None.
    */
   static void onNodeBoundary(const VirtualOp &op, NodeExecPhase phase) noexcept
   {
      DataProbeSession &s = session();

      if (s.state != DataProbeState::kDataProbeArmed) {
         return;
      }
      if ((phase != s.phase) || (op.getNodeId() != s.nodeId)) {
         return;
      }

      void *portAddress = (s.portType == DataProbePortType::kDataProbePortInput) ? op.getInputAddr(s.portIndex)
                                                                                 : op.getOutputAddr(s.portIndex);
      if (portAddress == nullptr) {
         // Node reports no address for this port: drop the request rather than
         // leave the session armed against a port that will never resolve.
         s.state = DataProbeState::kDataProbeIdle;
         return;
      }

      const uint8_t *src = static_cast<const uint8_t *>(portAddress);

      if (s.isInterleave) {
         src += (static_cast<size_t>(s.channel) * s.elementSize);
         for (size_t sample = 0U; sample < s.numSamples; ++sample) {
            (void) std::memcpy(&s.dest[sample * s.elementSize], &src[sample * s.pitchBytes], s.elementSize);
         }
      }
      else {
         (void) std::memcpy(s.dest, &src[static_cast<size_t>(s.channel) * s.pitchBytes], s.frameBytes);
      }

      s.bytesProbed   = s.frameBytes;
      s.state         = DataProbeState::kDataProbeReady;
   }

 private:
   /**
    *  @brief Accessor for the single graph-wide dataProbe session.
    *
    *  @details Header-only storage: one definition is emitted across all
    *           translation units that include this header.
    *  @retval Reference to the session state.
    */
   static DataProbeSession &session() noexcept
   {
      static DataProbeSession s{};
      return s;
   }
};

} // namespace TISP
