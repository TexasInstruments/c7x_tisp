cmake_minimum_required(VERSION 3.16.0)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/colorized_messages.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/clang-format.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/doxygen.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/username.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/utils.cmake)

# Build options
option(BUILD_LIB "Build only TISP and TISP_test_common libraries (no test executables)" OFF)

# Check if CGT7X_ROOT env variable is set
if(NOT DEFINED ENV{CGT7X_ROOT})
	resolve_linux_username(CURRENT_USER)
	if (CURRENT_USER STREQUAL "")
		message(FATAL_ERROR "User name not found")
	endif()
	# Use the actual HOME environment variable if available, fallback to construction
	if(DEFINED ENV{HOME})
		set(HOME "$ENV{HOME}")
	else()
		set(HOME "/home/${CURRENT_USER}")
	endif()
	set(ENV{CGT_C7X_VERSION} "6.2.1.LTS")
	set(_TOOLS_PATH "${HOME}/ti")
	set(_CGT7X_ROOT "${_TOOLS_PATH}/ti-cgt-c7000_$ENV{CGT_C7X_VERSION}")
	set(ENV{TOOLS_PATH} "${_TOOLS_PATH}")
	set(ENV{CGT7X_ROOT} "${_CGT7X_ROOT}")
	set(ENV{PATH} "$ENV{CGT7X_ROOT}/bin:$ENV{PATH}")
	message(
	STATUS
	"${Green} CGT7X_ROOT env variable is set.")
else()
	message(STATUS "HOME environment variable is already set: $ENV{CGT7X_ROOT}")
endif()

# ##############################################################################
# Locate MCU+ SDK — use find_path (CMake 3.16) to locate dmautils.
# Priority 1: West workspace — mcu_plus_sdk/ sibling fetched via dev.yml.
# Priority 2: MCU_PLUS_SDK_ROOT env var (e.g. source ~/.c7x_env_c7504).
# No legacy automatic fallback — explicit location required.
# ##############################################################################
get_filename_component(TISP_WORKSPACE_EARLY "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(MCU_SDK_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)

find_path(MCU_SDK_SOURCE_DIR
  NAMES "drivers/dmautils/dmautils.h"
  PATHS
    "${MCU_SDK_ROOT}/source"
    "${TISP_WORKSPACE_EARLY}/mcu_plus_sdk/source"
    "$ENV{MCU_PLUS_SDK_ROOT}/source"
    "$ENV{MCU_PLUS_SDK_ROOT}"
  NO_DEFAULT_PATH
)

if(NOT MCU_SDK_SOURCE_DIR)
  message(FATAL_ERROR
    "MCU+ SDK not found. Provide it via one of:\n"
    "  West (recommended): west init -l tisp/ --mf dev.yml && west update\n"
    "  Env var:            export MCU_PLUS_SDK_ROOT=/path/to/sdk")
endif()

message(STATUS "MCU+ SDK source: ${MCU_SDK_SOURCE_DIR}")

# set the udma directory
set(C7X_DMAUTILS_ROOT "${MCU_SDK_SOURCE_DIR}/drivers/dmautils")
set(C7X_DRIVERS_ROOT  "${MCU_SDK_SOURCE_DIR}/drivers/")
set(DMA_UTILS_INCLUDE_DIR "${MCU_SDK_SOURCE_DIR}/")

set(TISP_USE_INTERNEL "False")

# ##############################################################################
# Detect West workspace xlib siblings.
# tisp_configure_xlib() uses find_library to locate pre-built artifacts under
# TISP_WORKSPACE/mcu_plus_sdk/source/{name}/lib/{build_type}/ — FATAL_ERROR if not found.
# ##############################################################################
set(TISP_WORKSPACE "${TISP_WORKSPACE_EARLY}")
# xlib artifacts are expected at mcu_plus_sdk/source/*/lib/${CMAKE_BUILD_TYPE}/ under TISP_WORKSPACE.
# tisp_configure_xlib() will FATAL_ERROR with a clear message if not found.
# Pre-requisite: run 'west update && west build-tisp' to build xlib dependencies.

tisp_configure_xlib(FFTLIB HAS_COMMON)
tisp_configure_xlib(DSPLIB HAS_COMMON)
tisp_configure_xlib(AUDIOLIB HAS_COMMON PC_EXTRA_INCLUDES common)
tisp_configure_xlib(MATHLIB EXTRA_INCLUDES common)

# Set release as default build type
if(NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        "release"
        CACHE STRING "" FORCE)
  endif()
endif()

tisp_print_build_details()

if(TARGET_PLATFORM STREQUAL "PC")
  set(CMAKE_EXPORT_COMPILE_COMMANDS
      ON
      CACHE INTERNAL "")

endif()

set(CMAKE_CXX_STANDARD 14)

if(DEVICE STREQUAL "C7524")
  set(CGT_POSTFIX "-MMA2_256")
endif()

# ##############################################################################
# Pick the correct tool chain file of HE vs C7x ##
# ##############################################################################

if(TARGET_PLATFORM STREQUAL "PC")

  set(C7X_INCLUDES "$ENV{CGT7X_ROOT}/host_emulation/include/${DEVICE}${CGT_POSTFIX}")

  if(SOC STREQUAL AM62A)
    set(C7X_DMA_LIBS
        ${C7X_DMAUTILS_ROOT}/lib/dmautils.am62ax.c75x.ti-c7x-hostemu.release.lib
    )

  endif()

  if(SOC STREQUAL AM62D)
    set(C7X_DMA_LIBS
        ${C7X_DMAUTILS_ROOT}/lib/dmautils.am62dx.c75x.ti-c7x-hostemu.release.lib
    )
  endif()

  if(SOC STREQUAL AM275)
    set(C7X_DMA_LIBS
        ${C7X_DMAUTILS_ROOT}/lib/dmautils.am275x.c75x.ti-c7x-hostemu.release.lib
    )

  endif()

  if(SOC STREQUAL j722s)
    set(C7X_DMA_LIBS
        ${CMAKE_SOURCE_DIR}/ext_libs/dmautils.j722s.c75ssx-0.ti-c7x-hostemu.release.lib
    )
  endif()

  message(STATUS "C7X_DMA_LIBS: ${C7X_DMA_LIBS}")

  if(CMAKE_HOST_WIN32)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/msvc.cmake")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
  else()
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/gcc.cmake")
  endif()

else()

  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/c7x.cmake")
  set(C7X_INCLUDES "$ENV{CGT7X_ROOT}/include")

  string(TOLOWER ${SOC} SOC_LOWERCASE)
  string(TOLOWER ${DEVICE} DEVICE_LOWERCASE)
  # MCU SDK uses <soc>x naming (e.g. am62dx, am275x) for AM-type SOCs, bare name for j722s
  if(${SOC} STREQUAL "j722s")
    set(MCU_SDK_SOC_NAME "${SOC_LOWERCASE}")
  else()
    set(MCU_SDK_SOC_NAME "${SOC_LOWERCASE}x")
  endif()

  if(${SOC} STREQUAL "AM62A"
     OR ${SOC} STREQUAL "AM62D"
     OR ${SOC} STREQUAL "AM275")
    set(C7X_DMA_LIBS
        ${C7X_DMAUTILS_ROOT}/lib/dmautils.${SOC_LOWERCASE}x.c75x.ti-c7000.release.lib
    )
    set(C7X_FREE_RTOS_LIB
        ${MCU_SDK_SOURCE_DIR}/kernel/freertos/lib/freertos.${SOC_LOWERCASE}x.c75x.ti-c7000.release.lib
    )
    set(C7X_UDMA_LIB
        ${MCU_SDK_SOURCE_DIR}/drivers/udma/lib/udma.${SOC_LOWERCASE}x.c75x.ti-c7000.release.lib
    )
    set(C7X_DRIVERS_LIB
        ${C7X_DRIVERS_ROOT}/lib/drivers.${SOC_LOWERCASE}x.c75x.ti-c7000.release.lib
    )

    set(C7X_LIBC $ENV{CGT7X_ROOT}/lib/libc.a)
  endif()

  if(SOC STREQUAL j722s)
    set(C7X_DMA_LIBS
        ${C7X_DMAUTILS_ROOT}/lib/dmautils.${SOC_LOWERCASE}.c75ss0-0.ti-c7000.release.lib
    )

    set(C7X_FREE_RTOS_LIB
        ${MCU_SDK_SOURCE_DIR}/kernel/freertos/lib/freertos.${SOC_LOWERCASE}.c75x.ti-c7000.release.lib
    )
    # set(C7X_UDMA_LIB
    # $ENV{MCU_PLUS_SDK_ROOT}/source/drivers/udma/lib/udma.${SOC_LOWERCASE}.c75x.ti-c7000.release.lib
    # )
    set(C7X_DRIVERS_LIB
        ${C7X_DRIVERS_ROOT}/lib/drivers.${SOC_LOWERCASE}.c75ss0-0.ti-c7000.release.lib
    )

    set(C7X_LIBC $ENV{CGT7X_ROOT}/lib/libc.a)
  endif()

  include_directories(BEFORE SYSTEM $ENV{CGT7X_ROOT}/include)

endif()

list(APPEND C7X_INCLUDES ${FFTLIB_INCLUDES} ${DSPLIB_INCLUDES}
     ${AUDIOLIB_INCLUDES} ${MATHLIB_INCLUDES})

# Set compilation flags for release and debug based on TARGET_PLATFORM
tisp_compile_options()
tisp_include_xlib_headers()

project(TISP)

# Place all bins static libs in known place
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")

set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE INTERNAL "") # works
# ##############################################################################
# Create dependency based on testing a kernel or running the example for a given
# kernel #
# ##############################################################################

if(BUILD_TEST)

  add_subdirectory(test)

elseif(AUTO_TEST)

  set(BUILD_LIB 1)
  add_subdirectory(test)

elseif(BUILD_EXAMPLE)

  set(BUILD_LIB 1)
  add_subdirectory(examples)

elseif(BUILD_LIB)
  # Build only TISP library and TISP_test_common library (no test executables)
  add_subdirectory(test/common)

endif()

# Rules for building src directory
add_subdirectory(src bin)
