function(TISP_create_test_binary_name myKernelName)
  if(TARGET_PLATFORM STREQUAL "PC")
    set(exeName
        "test_${myKernelName}_${DEVICE}_x86_64"
        PARENT_SCOPE)
  else()
    set(exeName
        "test_${myKernelName}_${DEVICE}"
        PARENT_SCOPE)
  endif()
endfunction()

function(TISP_create_binary_for_kernel_testing)

  if(TARGET_PLATFORM STREQUAL "PC")
    file(GLOB_RECURSE SRC_CPP_FILES "*.cpp")
    file(GLOB_RECURSE SRC_C_FILES "*.c")
    file(GLOB_RECURSE SRC_H_FILES "*.h")
    set_source_files_properties(${SRC_C_FILES} PROPERTIES LANGUAGE CXX)
    list(APPEND SRC_FILES ${SRC_CPP_FILES} ${SRC_C_FILES} ${SRC_H_FILES})
  else()
    file(GLOB_RECURSE SRC_FILES "*.cpp" "*.c" "*.h")
  endif()
  get_filename_component(myKernelName "${CMAKE_CURRENT_LIST_DIR}" NAME)
  tisp_create_test_binary_name(${myKernelName})

  if(TARGET_PLATFORM STREQUAL PC)
    set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_OPTIONS "-w")

  else()
    set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_OPTIONS
                                                        "--no_warnings")
  endif()

  add_executable(${exeName} ${SRC_FILES})

  if(NOT TARGET_PLATFORM STREQUAL "PC")
    set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_OPTIONS
                                                        "--no_warnings")
  endif()

  target_include_directories(
    ${exeName}
    PUBLIC
      ${CMAKE_SOURCE_DIR}/includes
      ${C7X_INCLUDES}
      ${C7X_UDMA_ROOT}
      ${MCU_SDK_SOURCE_DIR}/
      ${MCU_SDK_SOURCE_DIR}/kernel/freertos/FreeRTOS-Kernel/include
      ${MCU_SDK_SOURCE_DIR}/kernel/freertos/portable/TI_CGT/DSP_C75X
      ${MCU_SDK_SOURCE_DIR}/kernel/freertos/config/${MCU_SDK_SOC_NAME}/c75x
      ${CMAKE_SOURCE_DIR}/test/common/generated/${DEVICE_LOWERCASE}
      ${CMAKE_SOURCE_DIR}/test/common
      ${CMAKE_SOURCE_DIR}/test/common/c71)

  tisp_set_bin_extension(${exeName})

  if(${TISP_USE_INTERNEL} STREQUAL "True")
    add_dependencies("${exeName}" ${TISP_DEPENDENCIES})
  endif()

  target_link_libraries(
    ${exeName}
    PRIVATE $<$<NOT:$<BOOL:${AUTO_TEST}>>:${extObjNames}>
            TISP
            TISP_test_common
            ${FFTLIB_LIBS}
            ${DSPLIB_LIBS}
            ${AUDIOLIB_LIBS}
            ${MATHLIB_LIBS}
            ${C7x_LIBS}
            ${C7X_DRIVERS_LIB}
            ${C7X_FREE_RTOS_LIB}
            ${C7X_UDMA_LIB}
            ${C7X_DMA_LIBS}
            ${C7X_LIBC})

  if(TARGET_PLATFORM STREQUAL "PC")
    if(CMAKE_HOST_UNIX)
      target_link_options(${exeName} PRIVATE -coverage -lgcov)
    endif()
  endif()

  if(NOT AUTO_TEST)
    tisp_add_coverage(${exeName})
    set(IDAT_DIR ${CMAKE_SOURCE_DIR}/test/TISP_idat_gen/${KERNEL_NAME})
    if(EXISTS ${IDAT_DIR})
      tisp_gen_test_cases(${myKernelName})
      add_dependencies(${exeName} gen_test_case_headers)
    endif()
  endif()

  if(${TEST_CASE})
    target_compile_definitions(${exeName} PUBLIC TEST_CASE=${TEST_CASE})
  endif()

  if(${TEST_CATEGORY})
    target_compile_definitions(${exeName} PUBLIC TEST_CATEGORY=${TEST_CATEGORY})
  endif()

  if(${ALL_TEST_CASES})
    target_compile_definitions(${exeName} PUBLIC ALL_TEST_CASES)
  elseif(${DEMO_CASE})
    target_compile_definitions(${exeName} PUBLIC DEMO_CASE)
  endif()

  if(NOT TARGET_PLATFORM STREQUAL "PC")
    tisp_set_linker_options_c7x(${exeName})
  endif()

  tisp_finish_compilation(${exeName})

endfunction()
