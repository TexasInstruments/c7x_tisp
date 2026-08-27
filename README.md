# TISP — TI Signal Processing Middleware

C++14 signal processing middleware for TI C7x DSPs (AM62DX, AM275x, J722S).

TISP provides a graph-based framework where signal processing **nodes** (`VirtualOp`
subclasses) are assembled into an **opVec** and executed sequentially.  Optional
**SuperNode** wrappers add DMA-driven L2SRAM block processing for throughput-critical
paths.

---

## Contents

- [Architecture](#architecture)
- [Node Library](#node-library)
- [Directory Structure](#directory-structure)
- [Dependencies](#dependencies)
- [Quick Start — West Workspace (recommended)](#quick-start--west-workspace-recommended)
- [Quick Start — Legacy SDK Build](#quick-start--legacy-sdk-build)
- [Build Reference](#build-reference)
- [Running Tests](#running-tests)
- [Arena Memory Configuration](#arena-memory-configuration)
- [Code Style](#code-style)
- [Golden Rules](#golden-rules)

---

## Architecture

### VirtualOp — abstract node base

Every signal processing node derives from `TISP::VirtualOp`:

```cpp
class VirtualOp {
public:
   virtual const char *getNodeName() const noexcept = 0;
   virtual uint32_t    getNodeId()   const noexcept = 0;
   virtual const char *getParams()   const noexcept = 0;
   virtual void        exec()              noexcept = 0;  // hot path
   virtual void        setAddr(SetAddr_t &, SetAddrMode_t) noexcept = 0;
};
```

All 63 nodes follow the same template: `template <typename T> class Name final : public VirtualOp`.

### opVec — signal processing graph

```cpp
using opVec = std::vector<std::unique_ptr<VirtualOp>>;
```

Build a graph by pushing nodes onto an `opVec`, then execute it:

```cpp
auto graph = std::make_unique<TISP::opVec>();
graph->push_back(std::make_unique<TISP::DSPLIB::CascadeBiquad<float>>(pIn, pOut, coeffs, nStages, nCh));
graph->push_back(std::make_unique<TISP::AUDIOLIB::Gain<float>>(pOut, pOut2, gain, nCh));

TISP::execute::graph(graph);  // calls exec() on each node with profiler
```

### SuperNode — DMA-driven block processing

SuperNodes wrap an `opVec` and execute it on L2SRAM block buffers while DMA
transfers data between DDR and L2SRAM in a ping-pong pipeline.  The embedded
`opVec` nodes have their I/O addresses rebound to L2SRAM via `setAddr()`.

Six variants are provided: `I2dToO2d`, `I2dToO2dDiffDims`, `I2dI2dToO2d`,
`I2dI2dToO2dO2d`, `I2dToO2dTranspose`, `i2d_to_o2dMatMul`.

### Memory model

| Region | Latency | Typical use |
|--------|---------|-------------|
| L2SRAM | 0 wait states | DMA buffers, twiddle factors, node objects |
| DDR | ~150 cycles | Bulk input/output frames |

Node objects are placed in L2SRAM via placement new + `L2BumpAllocator` so the
vtable pointer is zero-wait-state on every `exec()` call.

---

## Node Library

| Namespace | Count | Nodes |
|-----------|-------|-------|
| `TISP::AUDIOLIB` | 31 | Gain, Delay, Mux, Limiter, ASRC, NLMS, Mixer, Router, … |
| `TISP::MATHLIB` | 15 | Sin, Cos, Tan, Exp, Log, Sqrt, Div, … |
| `TISP::DSPLIB` | 10 | CascadeBiquad, Fir, MatMul, Interleave, AddNCh, … |
| `TISP::FFTLIB` | 3 | FFT1dBatched, RFFT1dBatched, IFFTR1dBatched |
| `TISP::misc` | 4 | BlockCopy, DataConvert, Dummy, zeroOutRFFT |
| `TISP::SuperNode` | 6 | DMA block-processing wrappers |

One header per node under `includes/`; master include: `#include "tisp.hpp"`.

---

## Directory Structure

```
tisp/
├── includes/           Headers: node APIs, VirtualOp, SuperNode, utilities
│   ├── tisp.hpp        Master include (all nodes + framework)
│   ├── TISP_virtualOp.hpp
│   └── TISP_*.hpp      One header per node
├── src/
│   ├── AUDIOLIB/       31 audio nodes
│   ├── DSPLIB/         10 DSP nodes
│   ├── FFTLIB/         3 FFT nodes
│   ├── MATHLIB/        15 math nodes
│   ├── misc/           4 utility nodes
│   ├── superNode/      SuperNode DMA implementations
│   └── common/         VirtualOp, L2BumpAllocator, profiler
├── test/               Mirrors src/ — one directory per node
│   └── common/         NodeFactory, UnitTestL2Heap, profiling, TI_compare_mem
├── cmake/
│   ├── c7x.cmake       TI cl7x toolchain (target)
│   ├── gcc.cmake       g++-11 toolchain (Linux host emulation)
│   ├── msvc.cmake      MSVC 2022 toolchain (Windows host emulation)
│   ├── xlibs.cmake     West workspace xlib ExternalProject integration
│   └── linkers/        Device linker scripts (C7100, C7120, C7504, C7524)
├── scripts/
│   └── west_ext.py     west build-tisp extension command
├── ext_libs/           Pre-built xlib binaries (fallback, non-West builds)
├── west.yml            West manifest (public) — xlibs + FreeRTOS + dmautils
├── dev.yml             West manifest (internal dev) — xlibs + MCU+ SDK deps
├── west-commands.yml   West extension command descriptor
└── CMakePresets.json   All supported build configurations
```

---

## Dependencies

TISP depends on four TI external libraries (xlibs): **FFTLIB**, **DSPLIB**,
**AUDIOLIB**, **MATHLIB**.  Two ways to provide them:

### Python build tooling (`west build-tisp`)

`west build-tisp` requires `west` and optionally `psutil`:

```bash
pip install west psutil
```

`psutil` enables memory-aware parallel build throttling — without it the
build falls back to CPU-count-only limits, which may OOM on machines with
less than ~2 GB RAM per (soc × platform × build-type) combo.

To pin outer combo parallelism manually (e.g. on machines with heavy
background load), set `TISP_MAX_WORKERS`:

```bash
TISP_MAX_WORKERS=2 west build-tisp   # run at most 2 combos in parallel
```

| Method | When to use |
|--------|-------------|
| **West workspace** (recommended) | New setups, CI/CD, repo-first workflow |
| **MCU+ SDK / PSDK_RTOS** (legacy) | Existing SDK installs |

---

## Quick Start — West Workspace (recommended)

### How the bootstrapping works

`west.yml` lives **inside this repo** (checked into git).  West uses it as a
manifest — a recipe declaring which sibling repos to fetch and at which
revisions.  The bootstrapping sequence is:

1. You clone `tisp/` manually with plain `git clone` — this is the only step
   that requires you to know the repo URL.
2. `west init -l tisp/` reads `west.yml` from the already-cloned repo and
   creates a `.west/config` in the parent directory, promoting that directory
   to a **West workspace root**.
3. `west update` fetches every project declared in the manifest as siblings of
   `tisp/` at the pinned revisions.
4. `west build-tisp` runs cmake configure + build.  CMake auto-detects the
   workspace, builds all xlibs in parallel, then builds TISP itself.

Two manifest files are provided — choose based on your needs:

| Manifest | Contents | `MCU_PLUS_SDK_ROOT` |
|----------|----------|---------------------|
| `west.yml` | xlibs + FreeRTOS + dmautils | must be set via env (`source ~/.c7x_env_c7504`) |
| `dev.yml` | xlibs + MCU+ SDK deps (via mcupsdk-manifests import) | **auto-detected** from workspace |

### Option A — Minimal (`west.yml`, xlibs only)

```bash
mkdir workspace && cd workspace
git clone https://github.com/TexasInstruments/c7x_tisp.git
pip install west

west init -l tisp/
west update               # clones fftlib, dsplib, audiolib, mathlib, FreeRTOS-Kernel, dmautils

source ~/.c7x_env_c7504   # sets CGT7X_ROOT, PATH, MCU_PLUS_SDK_ROOT
west build-tisp
```

### Option B — Full dev (`dev.yml`, xlibs + MCU+ SDK)

Only `CGT7X_ROOT` needed — `MCU_PLUS_SDK_ROOT` is auto-detected from the workspace.

**Every new workspace:**
```bash
mkdir workspace && cd workspace
git clone https://github.com/TexasInstruments/c7x_tisp.git
pip install west

west init -l tisp/ --mf dev.yml
west update               # clones xlibs + full MCU+ SDK deps (FreeRTOS, dmautils, drivers...)

export CGT7X_ROOT=/path/to/ti-cgt-c7000_<version>
west build-tisp           # MCU_PLUS_SDK_ROOT auto-set from workspace
```

On build you will see:
```
-- West workspace: MCU_PLUS_SDK_ROOT auto-set to /path/to/workspace/mcu_plus_sdk
-- West workspace detected — xlibs will be built from source
...
Done — am62d built successfully.
Done — am275 built successfully.
```

### `west build-tisp` options

```bash
west build-tisp                              # am62d + am275, pc, release, lib only
west build-tisp --soc am62d                  # single SOC
west build-tisp --type autotest              # library + tests
west build-tisp --platform target            # C7x target build
west build-tisp --build-type debug           # debug
west build-tisp --preset release-autotest-am62d-pc  # explicit preset override
```

Build directories are isolated per SOC and platform so switching between
host emulation and C7x target requires no `--fresh`:

```
tisp/build/
├── am62d/
│   ├── pc/        ← west build-tisp --soc am62d
│   └── target/    ← west build-tisp --soc am62d --platform target
└── am275/
    ├── pc/        ← west build-tisp --soc am275
    └── target/    ← west build-tisp --soc am275 --platform target
```

### Workspace layout after `west update`

**Option A (`west.yml`):**
```
workspace/
├── .west/config
├── tisp/
└── mcu_plus_sdk/
    └── source/
        ├── fftlib/
        ├── dsplib/
        ├── audiolib/
        ├── mathlib/
        ├── kernel/freertos/FreeRTOS-Kernel/
        └── drivers/dmautils/
```

**Option B (`dev.yml`):**
```
workspace/
├── .west/config
├── tisp/
└── mcu_plus_sdk/
    └── source/
        ├── fftlib/
        ├── dsplib/
        ├── audiolib/
        ├── mathlib/
        ├── kernel/freertos/FreeRTOS-Kernel/   ← from mcupsdk-manifests
        ├── drivers/dmautils/                  ← from mcupsdk-manifests
        └── ...                                ← additional MCU+ SDK deps
```

### Using TISP from a downstream project

A project that depends on TISP can import its manifest to get the full
dependency tree transitively:

```yaml
# downstream project's west.yml
projects:
  - name: tisp
    url: https://github.com/TexasInstruments/c7x_tisp.git
    revision: main
    import: true    # pulls in tisp/west.yml → fftlib, dsplib, audiolib, mathlib, FreeRTOS, dmautils
```

After `west update`, the downstream workspace has `tisp/` and all dependencies
under `mcu_plus_sdk/source/`.  `west build-tisp` is also available to the downstream project.

---

## Quick Start — Legacy SDK Build

```bash
export MCU_PLUS_SDK_ROOT=/path/to/freertos_sdk_<soc>_<version>
export CGT7X_ROOT=/path/to/ti-cgt-c7000_<version>

cmake -S . -B build --preset release-buildlib-am62d-pc --fresh
cmake --build build -j
```

On configure you will see:
```
-- Using MCU_PLUS_SDK xlibs (MCU_PLUS_SDK_ROOT = /path/to/sdk)
```

For J722S targets set `PSDK_RTOS_ROOT` instead of `MCU_PLUS_SDK_ROOT`.

---

## Build Reference

### Available presets

```bash
cmake -S . --list-presets
```

| Preset | Platform | SOC | Type |
|--------|----------|-----|------|
| `release-autotest-am62d-pc` | Linux host | AM62D / C7504 | Full test suite |
| `debug-autotest-am62d-pc` | Linux host | AM62D / C7504 | Full test suite (debug) |
| `release-autotest-am275-pc` | Linux host | AM275 / C7524 | Full test suite |
| `release-buildlib-am62d-pc` | Linux host | AM62D / C7504 | Library only |
| `release-autotest-am62d-target` | C7x target | AM62D / C7504 | Full test suite |
| `release-autotest-am275-target` | C7x target | AM275 / C7524 | Full test suite |
| … | | | |

### Output locations

| Artifact | Path |
|----------|------|
| TISP static library | `lib/release/TISP_<DEVICE>_x86_64.a` (PC) |
| TISP static library | `lib/release/TISP_<DEVICE>.lib` (target) |
| Test binaries (PC) | `bin/release/test_TISP_<name>_<DEVICE>_x86_64` |
| Test binaries (target) | `bin/release/test_TISP_<name>_<DEVICE>.out` |

### Platforms

| Platform | Compiler | C++ standard | Key defines |
|----------|----------|--------------|-------------|
| Linux host emulation | g++-11 | C++14 | `HOST_EMULATION` |
| Windows host emulation | MSVC 2022 | C++14 | `HOST_EMULATION`, `WIN32` |
| C7x target | cl7x (TI CGT) | C++14 (`--c++14`) | SOC-specific |

Use `#if defined(HOST_EMULATION)` to guard target-only code (DMA, L2SRAM sections, C7x intrinsics).

### Regenerate test data headers

If a build fails with missing `test_data/*.h`:

```bash
cmake --build build --target=gen_all_test_case_headers
```

---

## Running Tests

Build with an `autotest` preset, then run any binary from `bin/release/`:

```bash
cmake -S . -B build --preset release-autotest-am62d-pc --fresh
cmake --build build -j

./bin/release/test_TISP_fft1dBatched_C7504_x86_64
./bin/release/test_TISP_cascadeBiquad_C7504_x86_64
./bin/release/test_TISP_gain_C7504_x86_64
```

Each test binary exercises `create_graph()` (builds the processing graph) and
`execute_graph()` (runs it and compares output against a reference).

---

## Arena Memory Configuration

### Overview

TISP uses three bump-pointer arenas for deterministic memory allocation — no malloc at runtime. Arena buffers are defined by the application at link time via extern symbols. The library declares:

```cpp
extern uint8_t tisp_*ArenaBuffer[];
extern const size_t tisp_*ArenaSize;
```

Your application provides the definitions.

### The Three Arenas

| Arena | Symbol prefix | Alignment | Purpose |
|-------|--------------|-----------|---------|
| Node | `tisp_nodeArena` | 128 bytes | VirtualOp node objects (L2SRAM placement) |
| Twiddle | `tisp_twiddleArena` | 64 bytes | FFT twiddle/split-factor tables |
| Handle | `tisp_handleArena` | 32 bytes | xlib kernel handle allocations |

### Application Config File

Create a configuration file in your application (e.g., `TISP_app_arena_default_config.cpp`) that defines the three arena buffers:

```cpp
#if defined(HOST_EMULATION)
// Host emulation: allocate on heap for testing
static uint8_t s_nodeArenaBuffer[TISP_NODE_ARENA_SIZE];
static uint8_t s_twiddleArenaBuffer[TISP_TWIDDLE_ARENA_SIZE];
static uint8_t s_handleArenaBuffer[TISP_HANDLE_ARENA_SIZE];

uint8_t *tisp_nodeArenaBuffer = s_nodeArenaBuffer;
const size_t tisp_nodeArenaSize = sizeof(s_nodeArenaBuffer);

uint8_t *tisp_twiddleArenaBuffer = s_twiddleArenaBuffer;
const size_t tisp_twiddleArenaSize = sizeof(s_twiddleArenaBuffer);

uint8_t *tisp_handleArenaBuffer = s_handleArenaBuffer;
const size_t tisp_handleArenaSize = sizeof(s_handleArenaBuffer);

#else
// Target: section attributes map to L2SRAM in the linker script
__attribute__((section(".tisp_node_arena"), aligned(128)))
uint8_t tisp_nodeArenaBuffer[TISP_NODE_ARENA_SIZE];
const size_t tisp_nodeArenaSize = TISP_NODE_ARENA_SIZE;

__attribute__((section(".tisp_twiddle_arena"), aligned(64)))
uint8_t tisp_twiddleArenaBuffer[TISP_TWIDDLE_ARENA_SIZE];
const size_t tisp_twiddleArenaSize = TISP_TWIDDLE_ARENA_SIZE;

__attribute__((section(".tisp_handle_arena"), aligned(32)))
uint8_t tisp_handleArenaBuffer[TISP_HANDLE_ARENA_SIZE];
const size_t tisp_handleArenaSize = TISP_HANDLE_ARENA_SIZE;
#endif
```

**On target (C7x):** The section attributes (`.tisp_node_arena`, `.tisp_twiddle_arena`, `.tisp_handle_arena`) map to L2SRAM regions defined in the linker script.

**For apps without FFT:** You can opt out of the twiddle arena using a zero-length array:

```cpp
__attribute__((section(".tisp_twiddle_arena"), aligned(64)))
uint8_t tisp_twiddleArenaBuffer[0];
const size_t tisp_twiddleArenaSize = 0;
```

When size is 0, the twiddle arena is disabled and all twiddle allocations return nullptr.

### Sizing Guide

#### Manual (recommended for bring-up)

1. Start with 32 KB defaults for each arena
2. Build and run your graph
3. After `create_graph()`, call `TISP::printArenaStats()` to see peak usage:
   ```cpp
   auto graph = std::make_unique<TISP::opVec>();
   // ... populate graph ...
   TISP::printArenaStats();  // Shows bytes used / total for each arena
   ```
4. Size production buffers to observed peak × 1.5 (safety margin)

#### Automated (TVM codegen path)

Use the `tisp_sizeof` Python module (`tools/sizeof/`) to query node sizes programmatically:

```python
from tisp_sizeof import sizeof_node

# Sum node sizes for your graph
total = 0
for node_type in graph_nodes:
    total += sizeof_node(node_type) + 128  # 128B alignment padding per node
```

Then set `TISP_NODE_ARENA_SIZE = total`.

For handle arena, follow the same approach using xlib kernel handle sizes.

**Twiddle sizing (deterministic):**
- Sum twiddle table sizes for each distinct N-point FFT in your graph
- Shared: If multiple FFT nodes use the same N, the twiddle table is allocated once (read-only, shared)

### Linker Script

The target linker script defines L2SRAM sections that your section attributes target:

```
.tisp_node_arena    > L2SRAM ALIGN(0x80)
.tisp_twiddle_arena > L2SRAM ALIGN(0x40)
.tisp_handle_arena  > L2SRAM ALIGN(0x20)
```

These sections are defined in `cmake/linkers/<DEVICE>/lnk.cmd`. Your application can remap them to DDR if L2SRAM is constrained by editing the linker script.

### Error Handling

- **Null or zero size**: If buffer is null or `size` is 0, the arena is disabled — all allocations return nullptr
- **Alignment mismatch**: If buffer alignment doesn't meet arena requirements, the arena is disabled at graph-construction time
- **Runtime checks**: Use `allocator.isValid()` in your graph-create code to verify all arenas initialized correctly
- **Arena stats**: Call `TISP::printArenaStats()` to display `0/0` bytes for disabled arenas

---

## Code Style

Enforced by `cmake/.clang-format`:

| Rule | Value |
|------|-------|
| Indent | 3 spaces |
| Column limit | 120 |
| Brace style | Stroustrup |
| Pointer alignment | Right (`int *p`) |
| Include guard | `#pragma once` (never `#ifndef`) |

---

## Golden Rules

1. **C++14 only** — `--c++14` on cl7x, `-std=c++14` on g++. No C++17 features.
2. **Warnings are errors** — `--emit_warnings_as_errors` (target), `-Werror` (host).
3. **Every node**: `template <typename T> class Name final : public VirtualOp`.
4. **L2SRAM data**: `__attribute__((section(".l2sramData"), aligned(128)))` on target.
5. **Test entry points**: `void *create_graph()` + `void execute_graph(void *)`.
6. **Never force-push `dev`**.
7. **Feature branches**: `feature/TISP-<N>-<short-description>` branched from `dev`.
