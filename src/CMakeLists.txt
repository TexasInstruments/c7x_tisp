# add_subdirectory(common)

file(
  GLOB
  COMMON_CPP_FILES
  "*.cpp"
  "common/*.cpp"
  "FFTLIB/*.cpp"
  "DSPLIB/*.cpp"
  "AUDIOLIB/*.cpp"
  "MATHLIB/*.cpp"
  "superNode/*.cpp"
  "misc/*.cpp")
file(
  GLOB
  COMMON_H_FILES
  "*.hpp"
  "common/*.hpp"
  "FFTLIB/*.hpp"
  "DSPLIB/*.hpp"
  "AUDIOLIB/*.hpp"
  "MATHLIB/*.hpp"
  "misc/*.hpp"
  "superNode/*.hpp"
  "${CMAKE_SOURCE_DIR}/includes/*.hpp")

list(APPEND COMMON_FILES ${COMMON_CPP_FILES} ${COMMON_H_FILES})

add_library(TISP STATIC ${COMMON_FILES})

if(NOT TARGET_PLATFORM STREQUAL "PC")
  set_source_files_properties(
    ${COMMON_FILES} PROPERTIES COMPILE_OPTIONS "--emit_warnings_as_errors")
endif()

tisp_set_lib_extension(TISP)

if(${TISP_USE_INTERNEL} STREQUAL "True")
  add_dependencies(TISP ${TISP_DEPENDENCIES})
endif()

target_include_directories(
  TISP
  PUBLIC
    ${CMAKE_SOURCE_DIR}/includes
    ${C7X_INCLUDES}
    ${DMA_UTILS_INCLUDE_DIR}
    ${C7X_UDMA_ROOT}
    $ENV{MCU_PLUS_SDK_ROOT}/source/
    $ENV{MCU_PLUS_SDK_ROOT}/source/kernel/freertos/FreeRTOS-Kernel/include
    $ENV{MCU_PLUS_SDK_ROOT}/source/kernel/freertos/portable/TI_CGT/DSP_C75X
    $ENV{MCU_PLUS_SDK_ROOT}/source/kernel/freertos/config/${SOC_LOWERCASE}x/c75x
)
