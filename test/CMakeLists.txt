message(STATUS "                                                     ")
message(STATUS "-----------------------------------------------------")
message(STATUS "   Generating configs for testing ${KERNEL_NAME}     ")
message(STATUS "-----------------------------------------------------")
message(STATUS "                                                     ")

include(${CMAKE_SOURCE_DIR}/cmake/coverage.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/test.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/gen_test_cases.cmake)
add_subdirectory(common)

if(AUTO_TEST)

  set(TISP_TEST_DIRS
      FFTLIB
      DSPLIB
      misc
      MATHLIB
      audio_signal_chain
      AUDIOLIB
      superNode/i2d_to_o2d
      superNode/i2d_to_o2d_generic
      superNode/i2d_i2d_to_o2d
      superNode/i2d_i2d_to_o2d_o2d
      superNode/i2d_to_o2dTranspose)

  foreach(_test_dir IN LISTS TISP_TEST_DIRS)
    file(GLOB _subdirs "${CMAKE_CURRENT_LIST_DIR}/${_test_dir}/*")
    foreach(_subdir ${_subdirs})
      if(IS_DIRECTORY ${_subdir})
        get_filename_component(_name ${_subdir} NAME)
        if(NOT _name STREQUAL "TISP_idat_gen")
          message(STATUS "Adding kernel name ${_name}")
          add_subdirectory(${_subdir})
        endif()
      endif()
    endforeach()
  endforeach()

  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/superNode/TISP_matMul_superNode/)

else()
  add_subdirectory(${KERNEL_NAME})
endif()
