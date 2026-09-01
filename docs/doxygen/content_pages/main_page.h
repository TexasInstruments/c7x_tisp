/*!
    \mainpage TISP

    @attention
      - Features listed below are early versions and should be considered as "experimental".
      - Users can evaluate the feature, however the feature is not fully tested at TI side.
      - TI would not support these feature on public e2e.
      - Experimental features will be enabled with limited examples and SW modules.

    TISP (TI signal processing) is a software library that strives to abstract away the software and the
    hardware complexity of programming the C7x.


   \section Intro Introduction

   A typical signal chain is shown in the figure below. Basically, a collection of kernels (nodes) stitched together. A
   set of these kernels in the signal chain are grouped for DMA (supernode). Typically, in user applications, the
   complete data needs to be processed by nodes encapsulated inside the supernode before calling the next node in the
   signal chain. Nodes (kernels) themselves perform compute using data in L2SRAM. Data movement from  DDR to L2SRAM
   (DMA) is accomplished via DRU (data routing unit) available within the C7x.
   \image html typical_signal_chain.svg "Typical Signal Chain" width=500px

   \section node Node-level Abstraction
   In order to create a common baseline for kernels from  various libraries, we first create a virtual class
   @ref TISP::VirtualOp. Doing so, allows us to create a class for every kernel in a given
   library that is derived from the base virtual class @ref TISP::VirtualOp. Each node can be identified by a unique
   nodeName and nodeId, which are useful for debugging, profiling, and visualization purposes. Please note that
   node-level abstraction is not available for all kernels of a given library. Please refer to @ref dsplibNode for
   supported nodes.

   \image html node-level_abstraction.svg "Node-Level Abstraction"

   Employing this methodology enables us to string a bunch of nodes together through a custom vector datatype,
   @ref TISP::opVec. An example of the matrix transpose kernel from DSPLIB described as a class  @ref
   TISP::DSPLIB::MatTrans is shown below. Please refer to the DSPLIB documentation for more details about the data
   structures pertaining to DSPLIB.

   \snippet{lineno} TISP_matTrans.cpp DSPLIB_MatTrans constructor

   \section connectingNodes Connecting and Executing Nodes
   The data flow while connecting nodes is depicted through the pointer addresses shared between the nodes when creating
   a node through its class constructor. Whilst the compute flow is represented via the order in which the nodes are
   pushed back into @ref TISP::opVec. Thus allowing us to create any given signal chain as a graph.

   \image html connecting_nodes.svg "Connecting Nodes" width=500px

   An example code snippet is shown below for connecting creating and connecting nodes together, taken from @ref
   TISP_fft1dBatched_test.cpp. After creating nodes via constructors and pushing them back into @ref TISP::opVec with
   the desired order, @ref TISP::execute::graph can be called to iterate through the vector of nodes to call the
   execute function pertaining to a given node.

   \snippet{lineno} TISP_fft1dBatched_test.cpp connecting nodes example

   \section superNodes Supernode-level Abstraction
   We employ a similar strategy that was used to abstract nodes for supernodes as well. In the case of
   supernodes, we could have various variants such as @ref TISP::SuperNode::I2dToO2d, @ref
   TISP::SuperNode::I2dToO2dDiffDims, etc. Further styles of supernodes will be added as we expand the scope of this
   library/framework. Note that variants of supernode are derived from a non-virtual base class @ref
   TISP::SuperNode::BaseSuperNode.

   \image html supernode-level_abstraction.svg "Supernode-Level Abstraction" width=500px

   An example code snippet for one-input, one-output supernode is shown below. The DRU parameters are programmed to
   access the input and output DDR space in a 2D form.

   \snippet{lineno} TISP_i2d_to_o2d.cpp supernode example

   \section connectingSuperNodes Connecting and Executing Supernodes
   Connecting and execution supernodes is also similar to nodes. The execution order of the kernels inside the supernode
   is determined by the TISP::opVec datastructure passed to the supernode constructor as argument. Each supernode keeps
   track of the number of blocks it processes, which is useful for performance analysis and profiling.

   \image html connecting_supernodes.svg "Connecting Supernodes" width=1000px

   Please refer to 2D FFT example section below for more details.

   \section fft2d 2D FFT Example
   The example has two supernodes connected together as shown in the figure below.
   \image html 2dfft.svg "2D FFT Example" width=1000px
   Shown below is the code snippet that depicts the 2D FFT graph. The code snippet is taken from @ref
   TISP_fft2d_superNode_test.cpp.
   \snippet{lineno} TISP_fft2d_superNode_test.cpp 2D FFT Example

   The use of smart pointers allows us to destroy the graph and the memory allocated for the nodes within the graph with
   minimal code overhead.

   \section l2memory L2SRAM Memory Management

   On TI C7x DSPs, L2SRAM is the fastest on-chip memory (zero wait states), while DDR access incurs ~150 cycle
   latency. TISP manages L2SRAM allocation through a simple mechanism: you give it a byte array, and it hands back
   aligned chunks from that array on demand. No operating system heap, no fragmentation — just a pointer that moves
   forward through a fixed-size buffer.

   The class that implements this is @ref TISP::BumpAllocator. It is compliant with MISRA C++:2023 Rule 21.6.2,
   AUTOSAR A18-5-10, and SEI CERT MEM54-CPP.

   \subsection l2allocator Bump Allocator

   Think of @ref TISP::BumpAllocator like a roll of tape. You pull off what you need (allocate), and the roll
   gets shorter. You can rewind the whole roll at once (reset), but you cannot put a piece back in the middle.

   Each allocation returns a smart pointer (@ref TISP::ArenaPtr) that automatically releases its memory when it goes
   out of scope. You choose the alignment you need — 32-byte, 64-byte, or custom — depending on what the DSP
   vector unit requires.

   @code{.cpp}
   // Step 1: Declare a byte array in L2SRAM (the "roll of tape")
   __attribute__((section(".l2sramData"), aligned(128))) uint8_t myArena[64 * 1024];

   // Step 2: Hand it to the allocator
   TISP::BumpAllocator alloc(myArena, sizeof(myArena));

   // Step 3: Pull off what you need
   TISP::ArenaPtr<float> buf = alloc.allocate64ByteAligned<float>(1024);  // 1024 floats, 64-byte aligned
   @endcode

   \subsection l2arenas Arena Ownership Model

   TISP uses two separate arenas (byte arrays). Who owns each one determines who is responsible for sizing and
   declaring it:

   | Arena | Who owns it? | What is it for? |
   |-------|--------------|-----------------|
   | **Node arena** | Your application | You declare the byte array and decide how big it is based on your signal chain. |
   | **Twiddle arena** | TISP library | A fixed 32 KB internal array used by FFT nodes. You never touch it directly. |

   The library provides the allocator class (@ref TISP::BumpAllocator). Your application provides both the node
   arena memory and the twiddle arena memory — you declare the backing arrays and choose their section placement
   at link time. The function @ref TISP::twiddleAllocator() returns a reference to the allocator that FFT nodes
   use internally.

   On target hardware, the twiddle arena is typically placed in L2SRAM (e.g. `.tisp_twiddle_arena` section) for
   zero-wait-state access. On a PC (host emulation), the same externally-provided buffer compiles as a plain
   aligned array — same API, same behavior, no hardware dependency.

   \subsection l2twiddle Twiddle Factor Arena

   FFT algorithms need precomputed trigonometric constants called "twiddle factors." These are purely a function of
   the FFT size — compute them once, reuse them on every frame. TISP handles this internally so the application does
   not need to know anything about twiddle factor allocation.

   When you create an FFT node (e.g., FFT1dBatched), the constructor allocates twiddle factors from the
   library-owned twiddle arena behind the scenes:

   @code{.cpp}
   // Inside FFT1dBatched constructor (internal — app never calls this directly):
   _pW = twiddleAllocator().allocate64ByteAligned<T>(_numPoints * 2);
   tw_gen();  // Fill in the twiddle factor values
   @endcode

   The @ref TISP::twiddleAllocator() function is a one-liner that returns a reference to a pre-existing allocator.
   Here is the complete implementation — there is nothing hidden:

   @code{.cpp}
   // src/common/TISP_bump_allocator.cpp

   // The backing array is declared by the application (extern linkage).
   // See test/common/TISP_app_arena_default_config.cpp for the default definition:
   //   __attribute__((section(".tisp_twiddle_arena"), aligned(64)))
   //   uint8_t tisp_twiddleArenaBuffer[32U * 1024U];
   extern uint8_t tisp_twiddleArenaBuffer[];
   extern const size_t tisp_twiddleArenaSize;

   // Allocator that manages the byte array above — created once at program startup
   static BumpAllocator twiddleAllocatorInstance(tisp_twiddleArenaBuffer, tisp_twiddleArenaSize, kTwiddleAlignment);

   // Returns a reference to the allocator above — this is the entire function
   BumpAllocator &TISP::twiddleAllocator() { return twiddleAllocatorInstance; }
   @endcode

   Why this design?
   - **Always available**: the byte array and allocator exist for the entire program lifetime, so FFT twiddle
     factors are always valid — no use-after-free.
   - **No external dependencies**: pure C++14 — no TI SDK heap calls, no OS allocator, no fragmentation risk.
   - **Zero cost at runtime**: twiddle factors are accessed as plain pointers during signal processing.
     The smart-pointer bookkeeping only runs when you create or destroy an FFT node, not on every frame.

   \section profiling Performance Profiling
   TISP includes a comprehensive profiling system through the @ref NodeProfiler class (global scope) that allows you to measure and
   analyze the performance of both individual nodes and supernodes. The profiler captures cycle counts for each node
   execution and provides detailed analytics for performance optimization.

   Key profiling features include:
   - Cycle count measurement for individual nodes and supernodes
   - Node identification via nodeName and nodeId for easy tracking
   - Block count tracking for supernodes to analyze per-block performance
   - JSON output for detailed performance analysis
   - Excel report generation via the json_to_xlsx_converter.py tool

   The profiling system helps identify performance bottlenecks and provides insights into the efficiency of supernode
   execution by comparing actual cycles with theoretical cycles based on the sum of node cycles multiplied by the number
   of blocks processed.

   Below is a practical example from an audio signal chain that demonstrates creating nodes with names and IDs, and
   using the profiler to generate performance data:

   \subsection creating_graph Creating Graph
   \snippet{lineno} TISP_cascadeBiquad_FIR_test.cpp create_graph

   \subsection executing_graph Executing Graph
   \snippet{lineno} TISP_cascadeBiquad_FIR_test.cpp execute_graph

   \par Disclaimer

    See also \ref TI_DISCLAIMER.
 */
