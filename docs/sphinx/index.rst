:icon: material/home

TISP — TI Signal Processing
============================

TISP (TI Signal Processing) is a C++ middleware library that abstracts away the
software and hardware complexity of programming the **C7™+MMA** DSP architecture.
It provides node-level and supernode-level APIs that let you compose signal chains
from optimised kernels drawn from AUDIOLIB, DSPLIB, FFTLIB, MATHLIB, and other
TI libraries — while the framework handles DMA scheduling, L2SRAM allocation, and
performance profiling automatically.

.. note::

   Features listed below are early versions and should be considered
   **experimental**.  Users can evaluate the features; however, they are not
   fully tested at TI.  TI would not support these features on public e2e.
   Experimental features will be enabled with limited examples and SW modules.

.. grid:: 1 2 2 3
   :gutter: 3
   :class-container: ti-home-grid

   .. grid-item-card:: :si-icon:`material/cog` Architecture
      :link: architecture
      :link-type: doc

      How TISP composes nodes and supernodes into signal-chain graphs, and how
      L2SRAM, DMA, and profiling are managed for you.

   .. grid-item-card:: :si-icon:`material/rocket-launch` Getting Started
      :link: getting_started
      :link-type: doc

      Configure, build, and run — from PC host emulation to on-target EVM.

   .. grid-item-card:: :si-icon:`material/book-open-variant` API Reference
      :link: api/index
      :link-type: doc

      Every node and supernode class, grouped by library, with signatures and
      usage notes.

   .. grid-item-card:: :si-icon:`material/chart-line` Performance
      :link: perf/index
      :link-type: doc

      Measured supernode cycle counts on C7504 (AM62D) and C7524 (AM275).

   .. grid-item-card:: :si-icon:`material/file-tree` Repository Layout
      :link: repository_layout
      :link-type: doc

      A guided tour of the source tree — where headers, kernels, tests, and
      build files live.

   .. grid-item-card:: :si-icon:`material/scale-balance` License
      :link: license
      :link-type: doc

      TISP is distributed under the TI Text File License (TI-TFL).

.. toctree::
   :hidden:
   :maxdepth: 2

   architecture
   getting_started
   repository_layout
   api/index
   perf/index
   license
