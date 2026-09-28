// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "TISP_ErrorCtxt.hpp"
#include "TISP_types.hpp"
#include "TISP_virtualOp.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace TISP {

/**
   @brief Node boundary at which a dataProbe is taken.
 */
enum class NodeExecPhase : uint8_t {
   kNodeExecPre  = 0U, /**< Before the node's exec(). */
   kNodeExecPost = 1U  /**< After the node's exec(). */
};

/**
   @brief Node side the dataProbe reads.
 */
enum class DataProbePortType : uint8_t {
   kDataProbePortInput  = 0U, /**< Input port. */
   kDataProbePortOutput = 1U  /**< Output port. */
};

/**
   @brief Most items in one session.
 */
constexpr size_t kMaxDataProbeItems = 32U;

/**
   @brief What an item captures.
 */
enum class DataProbeType : uint8_t {
   kDataProbeRaw  = 1U, /**< Channel samples, every frameRate-th frame. */
   kDataProbeRms  = 2U, /**< RMS of the channel over one frame, float32, every frame. */
   kDataProbePeak = 3U  /**< Peak |x| of the channel over one frame, float32, every frame. */
};

/**
   @brief Session state.

   @verbatim
   off --start()--> idle --(pass starts)--> armed --(all items filled)--> ready --release(nextBase)--> idle
   stop(): any state --> off
   @endverbatim
 */
enum class DataProbeState : uint8_t {
   kDataProbeOff   = 0U, /**< No session. */
   kDataProbeIdle  = 1U, /**< Waiting for the next pass. */
   kDataProbeArmed = 2U, /**< Current pass is being captured. */
   kDataProbeReady = 3U  /**< Frame captured, held until release(). */
};

/**
   @brief One captured item.
 */
struct DataProbeItem {
   uint32_t nodeId{0U};          /**< Node ID. */
   uint16_t offset{0U};          /**< Destination offset from the session base. */
   uint8_t  portType{0U};        /**< @ref DataProbePortType. */
   uint8_t  portIndex{0U};       /**< Port index. */
   uint8_t  channel{0U};         /**< Channel within the port. */
   DataProbeType type{DataProbeType::kDataProbeRaw}; /**< What the item captures. */
};

/**
   @brief The single dataProbe session.
 */
struct DataProbeSession {
   DataProbeItem           items[kMaxDataProbeItems]{};          /**< Items. */
   uint8_t                *base{nullptr};                        /**< Destination buffer. */
   uint64_t                frameCount{0U};                       /**< Graph passes since start(). */
   uint64_t                frameIndex{0U};                       /**< Frame being captured or ready. */
   uint16_t                frameRate{1U};                        /**< Raw when frameIndex % frameRate == 0. */
   uint8_t                 numItems{0U};                         /**< Items in use. */
   uint8_t                 numRawItems{0U};                      /**< Raw items among them. */
   volatile uint8_t        remaining{0U};                        /**< Items left to fill in this frame. */
   volatile DataProbeState state{DataProbeState::kDataProbeOff}; /**< Session state. */
};

/**
   \class DataProbe
   @brief Captures node port data inside the graph loop: per item, raw samples,
          RMS or peak of one channel, into a buffer the caller owns.

   @details Ports are captured at the node boundary because inter-node buffers
            are reused once their last consumer has run. TISP counts graph
            passes and arms itself; the caller consumes each ready frame and
            calls @ref release. One session per executable.

   @note Not reentrant. @ref addItem validates each item against the graph;
         the caller lays out the destination offsets. RMS / peak support
         int8, int16, int32 and float32 ports.
 */
class DataProbe {
 public:
   /**
    *  @ingroup  DATA_PROBE
    */
   /**@{*/

   /**
    *  @brief Start configuring a session; clears previous items.
    *  @param [in] base      : Destination buffer, valid until @ref stop.
    *  @param [in] frameRate : Raw every frameRate-th frame (>= 1).
    *  @retval ErrorCode::kNone, kNullPointer if base is null, kDataProbeBusy if a
    *          session is running, or kInvalidParameter if frameRate is 0.
    */
   static ErrorCode configure(void *base, uint16_t frameRate) noexcept
   {
      DataProbeSession &s = session();

      ErrorCode status = ErrorCode::kNone;

      if (base == nullptr) {
         status = ErrorCode::kNullPointer;
      }
      else if (s.state != DataProbeState::kDataProbeOff) {
         status = ErrorCode::kDataProbeBusy;
      }
      else if (frameRate == 0U) {
         status = ErrorCode::kInvalidParameter;
      }
      else {
         s.numItems    = 0U;
         s.numRawItems = 0U;
         s.base        = static_cast<uint8_t *>(base);
         s.frameRate   = frameRate;
      }

      return status;
   }

   /**
    *  @brief Validate and add an item, between @ref configure and @ref start.
    *  @param [in]  &myOpVec  : Graph.
    *  @param [in]  nodeId    : Node ID.
    *  @param [in]  portType  : @ref DataProbePortType.
    *  @param [in]  portIndex : Port index.
    *  @param [in]  channel   : Channel within the port.
    *  @param [in]  type      : @ref DataProbeType value.
    *  @param [in]  offset    : Destination offset from base (any alignment).
    *  @param [out] &bytes    : Bytes the item writes per frame, set on success.
    *  @retval ErrorCode::kNone, kInvalidObjectId (unknown node),
    *          kInvalidParameter (bad port type, port, channel or type, or a
    *          port with no samples) or
    *          kMemory (@ref kMaxDataProbeItems in use).
    */
   static ErrorCode addItem(const opVec &myOpVec,
                            uint32_t     nodeId,
                            uint8_t      portType,
                            uint8_t      portIndex,
                            uint8_t      channel,
                            uint8_t      type,
                            uint16_t     offset,
                            size_t      &bytes) noexcept
   {
      DataProbeSession &s = session();

      ErrorCode        status       = ErrorCode::kNone;
      const bool       isInput      = (portType == static_cast<uint8_t>(DataProbePortType::kDataProbePortInput));
      const VirtualOp *op           = nullptr;
      Types            dataType     = Types::kInt32;
      size_t           pitchBytes   = 0U;
      bool             isInterleave = false;
      size_t           numChannels  = 0U;
      size_t           numSamples   = 0U;

      if ((portType > static_cast<uint8_t>(DataProbePortType::kDataProbePortOutput)) ||
          (type < static_cast<uint8_t>(DataProbeType::kDataProbeRaw)) ||
          (type > static_cast<uint8_t>(DataProbeType::kDataProbePeak))) {
         status = ErrorCode::kInvalidParameter;
      }
      else {
         op = findNode(myOpVec, nodeId);
         if (op == nullptr) {
            status = ErrorCode::kInvalidObjectId;
         }
         else if (portIndex >= (isInput ? op->getNumInputs() : op->getNumOutputs())) {
            status = ErrorCode::kInvalidParameter;
         }
         else {
            geometry(op->getNodeBufParams(), isInput, portIndex, dataType, pitchBytes, isInterleave, numChannels,
                     numSamples);
            if ((channel >= numChannels) || (numSamples == 0U)) {
               status = ErrorCode::kInvalidParameter;
            }
         }
      }

      if ((status == ErrorCode::kNone) && (s.numItems >= kMaxDataProbeItems)) {
         status = ErrorCode::kMemory;
      }

      if (status == ErrorCode::kNone) {
         const DataProbeType itemType = static_cast<DataProbeType>(type);
         bytes = (itemType == DataProbeType::kDataProbeRaw) ? (numSamples * static_cast<size_t>(sizeOf(dataType)))
                                                            : sizeof(float);
         DataProbeItem &it = s.items[s.numItems];
         it.nodeId    = nodeId;
         it.offset    = offset;
         it.portType  = portType;
         it.portIndex = portIndex;
         it.channel   = channel;
         it.type      = itemType;
         ++s.numItems;
         if (itemType == DataProbeType::kDataProbeRaw) {
            ++s.numRawItems;
         }
      }

      return status;
   }

   /**
    *  @brief Start capturing; the next graph pass is frame 0.
    */
   static void start() noexcept
   {
      DataProbeSession &s = session();
      s.frameCount = 0U;
      // Last, so the graph loop never sees a half set-up session.
      s.state      = DataProbeState::kDataProbeIdle;
   }

   /**
    *  @brief Stop capturing.
    */
   static void stop() noexcept { session().state = DataProbeState::kDataProbeOff; }

   /**
    *  @retval true if a captured frame is waiting.
    */
   static bool isReady() noexcept { return (session().state == DataProbeState::kDataProbeReady); }

   /**
    *  @retval Index of the ready frame, in graph passes since @ref start.
    */
   static uint64_t frameIndex() noexcept { return session().frameIndex; }

   /**
    *  @retval true if the ready frame includes raw samples.
    */
   static bool rawCaptured() noexcept { return isRawFrame(session()); }

   /**
    *  @brief Release the ready frame; the next one is captured at nextBase.
    *  @param [in] nextBase : Destination buffer for the next frame (may be the
    *                         same buffer). Item offsets are relative to it.
    *  @retval ErrorCode::kNone, kNullPointer (nextBase is null; the frame stays
    *          ready) or kInvalidParameter (no ready frame to release).
    */
   static ErrorCode release(void *nextBase) noexcept
   {
      DataProbeSession &s = session();

      ErrorCode status = ErrorCode::kNone;

      if (nextBase == nullptr) {
         status = ErrorCode::kNullPointer;
      }
      else if (s.state != DataProbeState::kDataProbeReady) {
         status = ErrorCode::kInvalidParameter;
      }
      else {
         s.base  = static_cast<uint8_t *>(nextBase);
         s.state = DataProbeState::kDataProbeIdle;
      }

      return status;
   }

   /** @} */

   /**
    *  @brief Called by @ref execute::graph before each top-level pass.
    *
    *  @details Counts the pass and, unless a frame is held, arms it with the
    *           number of items to fill. Passes while a frame is held are skipped.
    */
   static void onFrameStart() noexcept
   {
      DataProbeSession &s = session();

      if (s.state == DataProbeState::kDataProbeOff) {
         return;
      }

      const uint64_t frame = s.frameCount;
      s.frameCount         = frame + 1U;

      if (s.state == DataProbeState::kDataProbeReady) {
         return; // frame still held: skip this pass
      }

      s.frameIndex = frame;

      // Raw items only have work on raw frames.
      const uint8_t count = isRawFrame(s) ? s.numItems : static_cast<uint8_t>(s.numItems - s.numRawItems);

      s.remaining = count;
      // Last, so the node boundaries never see a stale count.
      s.state     = (count != 0U) ? DataProbeState::kDataProbeArmed : DataProbeState::kDataProbeIdle;
   }

   /**
    *  @brief Called by @ref execute::graph before and after each node: fills
    *         the items of that node and marks the frame ready when all are done.
    *  @param [in] &op   : Node.
    *  @param [in] phase : Before or after exec().
    */
   static void onNodeBoundary(const VirtualOp &op, NodeExecPhase phase) noexcept
   {
      DataProbeSession &s = session();

      if (s.state != DataProbeState::kDataProbeArmed) {
         return;
      }

      const uint32_t nodeId   = op.getNodeId();
      const bool     rawFrame = isRawFrame(s);

      for (size_t i = 0U; i < s.numItems; ++i) {
         const DataProbeItem &it = s.items[i];

         // Inputs are read before exec() (a kernel may overwrite them), outputs after.
         const bool          isInput = (it.portType == static_cast<uint8_t>(DataProbePortType::kDataProbePortInput));
         const NodeExecPhase valid   = isInput ? NodeExecPhase::kNodeExecPre : NodeExecPhase::kNodeExecPost;
         const bool          isRaw   = (it.type == DataProbeType::kDataProbeRaw);

         if ((phase != valid) || (nodeId != it.nodeId) || (isRaw && !rawFrame)) {
            continue;
         }

         void *portAddress = isInput ? op.getInputAddr(it.portIndex) : op.getOutputAddr(it.portIndex);
         if (portAddress == nullptr) {
            s.state = DataProbeState::kDataProbeIdle; // drop this frame
            return;
         }

         Types  dataType     = Types::kInt32;
         size_t pitchBytes   = 0U;
         bool   isInterleave = false;
         size_t numChannels  = 0U;
         size_t numSamples   = 0U;
         geometry(op.getNodeBufParams(), isInput, it.portIndex, dataType, pitchBytes, isInterleave, numChannels,
                  numSamples);

         const size_t   es     = static_cast<size_t>(sizeOf(dataType));
         const size_t   stride = isInterleave ? pitchBytes : es;
         const uint8_t *chSrc  = static_cast<const uint8_t *>(portAddress)
                                 + (static_cast<size_t>(it.channel) * (isInterleave ? es : pitchBytes));
         uint8_t       *dst    = &s.base[it.offset];

         if (!isRaw) {
            measure(chSrc, stride, numSamples, dataType, it.type, dst);
         }
         else if (isInterleave) {
            for (size_t k = 0U; k < numSamples; ++k) {
               (void) std::memcpy(&dst[k * es], &chSrc[k * stride], es);
            }
         }
         else {
            (void) std::memcpy(dst, chSrc, numSamples * es);
         }

         s.remaining = s.remaining - 1U;
      }

      if (s.remaining == 0U) {
         s.state = DataProbeState::kDataProbeReady;
      }
   }

 private:
   /**
    *  @retval true if the current frame is a raw frame.
    */
   static bool isRawFrame(const DataProbeSession &s) noexcept { return ((s.frameIndex % s.frameRate) == 0U); }

   /**
    *  @brief Sum of x^2 and max |x| over n samples of type T.
    */
   template <typename T>
   static void accumulate(const uint8_t *src, size_t stride, size_t n, float &sumSq, float &maxAbs) noexcept
   {
      float s = 0.0F;
      float m = 0.0F;

      for (size_t k = 0U; k < n; ++k) {
         const T     x = *reinterpret_cast<const T *>(&src[k * stride]);
         const float v = static_cast<float>(x);
         const float a = std::fabs(v);
         s += v * v;
         m = (a > m) ? a : m;   // select, not std::fmax (a library call on C7x)
      }

      sumSq  = s;
      maxAbs = m;
   }

   /**
    *  @brief Write the RMS or peak of one channel to dst as float32.
    */
   static void measure(const uint8_t *src, size_t stride, size_t n, Types t, DataProbeType type, uint8_t *dst) noexcept
   {
      float sumSq  = 0.0F;
      float maxAbs = 0.0F;

      if (t == Types::kInt8) {
         accumulate<int8_t>(src, stride, n, sumSq, maxAbs);
      }
      else if (t == Types::kInt16) {
         accumulate<int16_t>(src, stride, n, sumSq, maxAbs);
      }
      else if (t == Types::kInt32) {
         accumulate<int32_t>(src, stride, n, sumSq, maxAbs);
      }
      else {
         accumulate<float>(src, stride, n, sumSq, maxAbs);
      }

      const float value = (type == DataProbeType::kDataProbeRms)
                             ? std::sqrt(sumSq * (1.0F / static_cast<float>(static_cast<uint32_t>(n))))
                             : maxAbs;
      (void) std::memcpy(dst, &value, sizeof(value));
   }

   /**
    *  @brief Port geometry from a node's buffer parameters.
    */
   static void geometry(const NodeBufParams &bufParams, bool isInput, size_t portIndex, Types &dataType,
                        size_t &pitchBytes, bool &isInterleave, size_t &numChannels, size_t &numSamples) noexcept
   {
      if (isInput) {
         dataType     = bufParams.inputDataType;
         pitchBytes   = bufParams.inPitchBytes[portIndex];
         isInterleave = (bufParams.isInputInterleave != 0U);
         numChannels  = isInterleave ? bufParams.widthIn[portIndex] : bufParams.heightIn[portIndex];
         numSamples   = isInterleave ? bufParams.heightIn[portIndex] : bufParams.widthIn[portIndex];
      }
      else {
         dataType     = bufParams.outputDataType;
         pitchBytes   = bufParams.outPitchBytes[portIndex];
         isInterleave = (bufParams.isOutputInterleave != 0U);
         numChannels  = isInterleave ? bufParams.widthOut[portIndex] : bufParams.heightOut[portIndex];
         numSamples   = isInterleave ? bufParams.heightOut[portIndex] : bufParams.widthOut[portIndex];
      }
   }

   /**
    *  @retval Node with the given ID, or nullptr.
    */
   static const VirtualOp *findNode(const opVec &myOpVec, uint32_t nodeId) noexcept
   {
      const VirtualOp *op = nullptr;
      for (size_t i = 0U; i < myOpVec.size(); ++i) {
         if (myOpVec[i]->getNodeId() == nodeId) {
            op = myOpVec[i].get();
            break;
         }
      }
      return op;
   }

   /**
    *  @retval The session (header-only static storage).
    */
   static DataProbeSession &session() noexcept
   {
      static DataProbeSession s{};
      return s;
   }
};

} // namespace TISP
