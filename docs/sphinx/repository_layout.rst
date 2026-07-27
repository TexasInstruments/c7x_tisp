:icon: material/file-tree

Repository Layout
=================

Top-level structure of the TISP repository:

.. raw:: html

   <style>
   .repo-tree summary::-webkit-details-marker { display: none; }
   .repo-tree summary::before {
     content: "\25B6";
     font-size: 0.65em;
     color: #007C8C;
     flex-shrink: 0;
     transition: transform 0.15s;
     margin-right: 0.3em;
   }
   .repo-tree details[open] > summary::before { transform: rotate(90deg); }
   .rt-root {
     padding: 0.4rem 0.8rem;
     background: #115566;
     color: #ffffff;
     font-weight: 700;
     font-size: 0.88rem;
     letter-spacing: 0.03em;
     border-radius: 3px;
     margin-bottom: 0.3em;
   }
   .rt-l1 { padding-left: 1.2rem; }
   .rt-l2 { padding-left: 2.4rem; }
   details.rt-folder > summary {
     font-weight: 600;
     color: #0D3040;
     cursor: pointer;
     display: flex;
     align-items: baseline;
     gap: 0.3em;
     padding: 0.15em 0;
   }
   details.rt-folder > summary .rt-name { color: #007C8C; }
   details.rt-folder > summary .rt-desc {
     color: #547880;
     font-weight: 400;
     font-size: 0.8rem;
   }
   .rt-file {
     display: flex;
     align-items: baseline;
     gap: 0.4em;
     padding: 0.15em 0.5em;
     border-radius: 3px;
   }
   .rt-file:hover { background: #EAF4F6; }
   .rt-file .rt-icon { color: #547880; font-size: 0.85em; }
   .rt-file .rt-name { color: #0D3040; }
   .rt-file .rt-desc { color: #547880; font-size: 0.79rem; margin-left: 0.2em; }
   details.rt-folder[open] > .rt-children {
     border-left: 1px solid #E0E0E0;
     margin-left: 1.4rem;
     padding-left: 0.5rem;
   }
   </style>

   <div class="repo-tree">
   <div class="rt-root">tisp/</div>

   <!-- includes/ -->
   <details class="rt-folder rt-l1" open>
     <summary><span class="rt-name">includes/</span> <span class="rt-desc">Headers: node APIs, VirtualOp, SuperNode, utilities</span></summary>
     <div class="rt-children">
       <div class="rt-file rt-l2"><span class="rt-icon">&#128196;</span><span class="rt-name">tisp.hpp</span><span class="rt-desc">Master include (all nodes + framework)</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128196;</span><span class="rt-name">TISP_virtualOp.hpp</span><span class="rt-desc">VirtualOp base class</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128196;</span><span class="rt-name">TISP_*.hpp</span><span class="rt-desc">One header per node</span></div>
     </div>
   </details>

   <!-- src/ -->
   <details class="rt-folder rt-l1" open>
     <summary><span class="rt-name">src/</span></summary>
     <div class="rt-children">
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">AUDIOLIB/</span><span class="rt-desc">Audio nodes</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">DSPLIB/</span><span class="rt-desc">DSP nodes</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">FFTLIB/</span><span class="rt-desc">FFT nodes</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">MATHLIB/</span><span class="rt-desc">Math nodes</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">misc/</span><span class="rt-desc">Utility nodes</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">superNode/</span><span class="rt-desc">SuperNode DMA implementations</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">common/</span><span class="rt-desc">BumpAllocator, VirtualOp base, profiler</span></div>
     </div>
   </details>

   <!-- test/ -->
   <details class="rt-folder rt-l1" open>
     <summary><span class="rt-name">test/</span> <span class="rt-desc">Mirrors src/ &#8212; one directory per node</span></summary>
     <div class="rt-children">
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">common/</span><span class="rt-desc">NodeFactory, UnitTestL2Heap, profiling, TI_compare_mem</span></div>
     </div>
   </details>

   <!-- cmake/ -->
   <details class="rt-folder rt-l1" open>
     <summary><span class="rt-name">cmake/</span></summary>
     <div class="rt-children">
       <div class="rt-file rt-l2"><span class="rt-icon">&#128196;</span><span class="rt-name">c7x.cmake</span><span class="rt-desc">TI cl7x toolchain (target)</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128196;</span><span class="rt-name">gcc.cmake</span><span class="rt-desc">g++-11 toolchain (Linux host emulation)</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128196;</span><span class="rt-name">msvc.cmake</span><span class="rt-desc">MSVC 2022 toolchain (Windows host emulation)</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">linkers/</span><span class="rt-desc">Device linker scripts (C7100, C7120, C7504, C7524)</span></div>
     </div>
   </details>

   <!-- docs/ -->
   <details class="rt-folder rt-l1" open>
     <summary><span class="rt-name">docs/</span></summary>
     <div class="rt-children">
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">doxygen/</span><span class="rt-desc">Doxyfiles, content pages, performance HTML</span></div>
       <div class="rt-file rt-l2"><span class="rt-icon">&#128193;</span><span class="rt-name">sphinx/</span><span class="rt-desc">Sphinx site (conf.py, rst, TI theme)</span></div>
     </div>
   </details>

   <!-- top-level files -->
   <div class="rt-file rt-l1"><span class="rt-icon">&#128196;</span><span class="rt-name">CMakePresets.json</span><span class="rt-desc">All supported build configurations</span></div>
   <div class="rt-file rt-l1"><span class="rt-icon">&#128196;</span><span class="rt-name">west.yml</span><span class="rt-desc">West manifest (public) &#8212; xlibs + FreeRTOS + dmautils</span></div>
   <div class="rt-file rt-l1"><span class="rt-icon">&#128196;</span><span class="rt-name">dev.yml</span><span class="rt-desc">West manifest (internal dev) &#8212; xlibs + MCU+ SDK deps</span></div>
   <div class="rt-file rt-l1"><span class="rt-icon">&#128196;</span><span class="rt-name">west-commands.yml</span><span class="rt-desc">West extension command descriptor</span></div>

   </div>

Node sources -- ``src/<LIB>/<Node>/``
--------------------------------------

Each node implementation lives in its own source folder under the appropriate
library sub-directory.  The public header in ``includes/`` carries the Doxygen
documentation that this site renders.

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - File
     - Contents
   * - ``includes/TISP_<Node>.hpp``
     - Public API -- constructor, exec, getter prototypes and Doxygen documentation
   * - ``src/<LIB>/TISP_<Node>.cpp``
     - Implementation (C++14, TI intrinsics where applicable)
   * - ``src/common/``
     - Cross-node definitions: ``TISP_common.hpp``, ``TISP_virtualOp.hpp``,
       ``TISP_bump_allocator.hpp``, ``TISP_profiler.hpp``

Tests -- ``test/<LIB>/TISP_<Node>/``
--------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - File
     - Contents
   * - ``TISP_<Node>_test.cpp``
     - Test driver (create_graph / execute_graph pattern)
   * - ``main_rtos.c``
     - RTOS entry point for on-target runs

Other folders
-------------

- ``cmake/`` -- build-system modules: toolchain files (``c7x.cmake``,
  ``gcc.cmake``, ``msvc.cmake``), documentation helper (``doxygen.cmake``),
  and test utilities.
- ``docs/doxygen/`` -- Doxygen configuration, narrative content pages, and the
  prebuilt performance HTML tables.
- ``docs/sphinx/`` -- this documentation site (``conf.py``, ``.rst`` sources, and
  the TI-branded theme assets under ``_static/``).
