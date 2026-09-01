// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// clang-format off

/**

 \page build_instructions_linux Ubuntu 22.04 Build Instructions for TISP

 \par Supported Platforms

 | SOC   | Device | EVM           | Boot Mode     |
 |-------|--------|---------------|---------------|
 | AM62D | C7504  | am62dx-evm    | OSPI NOR Boot |
 | AM275 | C7524  | am275x-evm    | SD Card Boot  |

 \par Host Dependencies

 - Install the following packages via apt (Ubuntu 22.04):
   \code
   sudo apt-get install lcov doxygen
   \endcode
   - \c lcov — required for code coverage (host PC builds)
   - \c doxygen — optional, needed to build the user guide (\c user_guide target)

 \par Python3 Setup

 - Test cases are auto-generated via python scripts packaged in the library

 - Please ensure that you have the following python3 packages installed

   - numpy, scipy, scikit-learn, pandas, matplotlib, soundfile, openpyxl, xlrd, and pyyaml


 \par Environment Setup

  - Run the executables on EVM using CCS (Code Composer Studio)

  - Setup needed for the compiler for CGT7x
    - Download [CGT7x Compiler Installer](https://www.ti.com/tool/C7000-CGT) and install
    - export CGT7X_ROOT=${CGT7X_COMPILER_DIR}

  - Setup MCU_PLUS_SDK_ROOT environment variable
    - export MCU_PLUS_SDK_ROOT=${MCU_PLUS_SDK_ROOT_DIR}
    - Example (AM62D): export MCU_PLUS_SDK_ROOT=${HOME}/freertos_sdk_am62dx_12_00_00_06
    - Example (AM275): export MCU_PLUS_SDK_ROOT=${HOME}/freertos_sdk_am275x_12_00_00_06

  - Path setup
    - export PATH=${CGT7X_ROOT}/bin:${PATH}

 \par Build using CMake Presets (Recommended)

  TISP utilizes CMake (v3.21+) with a presets system. Presets eliminate the need to
  pass long lists of -D flags on every configure invocation.

  \b CMakePresets.json ships with the package and defines the baseline presets.
  Users can create a \b CMakeUserPresets.json (gitignored, not shipped) for
  single-kernel development workflows.

  - List all available presets:
    \code
    cmake --list-presets
    \endcode

  - \b "Auto-test" presets build all test cases and the TISP library:

    | Preset Name                       | Platform | Build Type |
    |-----------------------------------|----------|------------|
    | release-autotest-am62d-target     | Target   | Release    |
    | debug-autotest-am62d-target       | Target   | Debug      |
    | release-autotest-am62d-pc         | Host PC  | Release    |
    | debug-autotest-am62d-pc           | Host PC  | Debug      |
    | release-autotest-am275-target     | Target   | Release    |
    | debug-autotest-am275-target       | Target   | Debug      |
    | release-autotest-am275-pc         | Host PC  | Release    |
    | debug-autotest-am275-pc           | Host PC  | Debug      |

  - \b "Build-lib" presets build only the TISP library (no tests):

    | Preset Name                       | Platform | Build Type |
    |-----------------------------------|----------|------------|
    | release-buildlib-am62d-target     | Target   | Release    |
    | release-buildlib-am275-target     | Target   | Release    |
    | release-buildlib-am62d-pc         | Host PC  | Release    |
    | release-buildlib-am275-pc         | Host PC  | Release    |

  - Example: build all tests for AM62D target in release mode:
    \code
    cmake -S . -B build --preset=release-autotest-am62d-target
    cmake --build build -j$(nproc)
    \endcode

  - Example: build only the TISP library for AM275 host emulation:
    \code
    cmake -S . -B build --preset=release-buildlib-am275-pc
    cmake --build build -j$(nproc)
    \endcode

 \par User Presets for Single-Kernel Development

  For iterative development on a single kernel, create a \b CMakeUserPresets.json
  in the TISP root directory. This file is gitignored and will not be shipped.

  Example CMakeUserPresets.json:
  \code{.json}
  {
    "version": 3,
    "cmakeMinimumRequired": { "major": 3, "minor": 21, "patch": 0 },
    "configurePresets": [
      {
        "name": "my-kernel-am62d-target",
        "inherits": "base-am62d-target",
        "cacheVariables": {
          "BUILD_TEST": "1",
          "KERNEL_NAME": "FFTLIB/TISP_fft2d_superNode",
          "ALL_TEST_CASES": "1",
          "CMAKE_BUILD_TYPE": "release"
        }
      },
      {
        "name": "my-kernel-am62d-pc",
        "inherits": "base-am62d-pc",
        "cacheVariables": {
          "BUILD_TEST": "1",
          "KERNEL_NAME": "misc/TISP_blockCopy_superNode",
          "CMAKE_BUILD_TYPE": "release",
          "TISP_DEBUGPRINT": "1"
        }
      },
      {
        "name": "my-kernel-am275-target",
        "inherits": "base-am275-target",
        "cacheVariables": {
          "BUILD_TEST": "1",
          "KERNEL_NAME": "FFTLIB/TISP_fft2d_superNode",
          "ALL_TEST_CASES": "1",
          "CMAKE_BUILD_TYPE": "release"
        }
      }
    ]
  }
  \endcode

  Then configure and build with:
  \code
  cmake -S . -B build --preset=my-kernel-am62d-target
  cmake --build build -j$(nproc)
  \endcode

  \note The \c inherits field references hidden base presets from CMakePresets.json
  (e.g., \c base-am62d-target, \c base-am275-pc). These set SOC, DEVICE, and
  TARGET_PLATFORM automatically so you only need to specify the kernel-specific options.

 \par Pre-packaged Libraries

  The TISP SDK ships with pre-built libraries under the source tree so you
  can link out of the box without building:

  - \c lib/release/TISP_\<device\>.lib — pre-built TISP library (target)
  - \c lib/release/TISP_\<device\>_x86_64.a — pre-built TISP library (host emulation)
  - \c lib/release/TISP_test_common_\<device\>.lib — test common library (target)
  - \c lib/release/TISP_test_common_\<device\>_x86_64.a — test common library (host emulation)

  These are built from the same source and can be used directly for linking
  without running CMake.

 \par Run

  - To run TISP kernels on a PC (host emulation):
    \code
    bin/release/test_TISP_<kernel_name>_<device>_x86_64
    \endcode
    - Example: \code bin/release/test_TISP_blockCopy_superNode_C7504_x86_64 \endcode

  - To run TISP kernels on target:
    - AM62D: run the .out binary on the EVM in OSPI NOR Boot Mode
    - AM275: run the .out binary on the EVM in SD Card Boot Mode
    - Use CCS to load and execute on the C7x DSP core

*/

// clang-format on
