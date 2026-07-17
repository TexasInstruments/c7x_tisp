# AUDIOLIB_idat_gen is in the audiolib source tree.
# In a West workspace audiolib lives at workspace/mcu_plus_sdk/source/audiolib/;
# in the legacy SDK it lives at MCU_PLUS_SDK_ROOT/source/audiolib/.
# Prepend both candidate paths to PYTHONPATH so the 20+ gen_data.py scripts
# find AUDIOLIB_idat_gen without any source modifications.
get_filename_component(_tisp_workspace "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
set(_audiolib_test_west   "${_tisp_workspace}/mcu_plus_sdk/source/audiolib/test")
set(_audiolib_test_legacy "$ENV{MCU_PLUS_SDK_ROOT}/source/audiolib/test")
set(_idat_pythonpath "${_audiolib_test_west}:${_audiolib_test_legacy}:$ENV{PYTHONPATH}")

function(tisp_gen_test_cases kernelName)

  add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/test/${KERNEL_NAME}/${kernelName}_idat.cpp"
           "${CMAKE_SOURCE_DIR}/test/${KERNEL_NAME}/${kernelName}_idat.c"
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${_idat_pythonpath}
            python3
            ${CMAKE_SOURCE_DIR}/test/TISP_idat_gen/${KERNEL_NAME}/gen_data.py
    DEPENDS
      ${CMAKE_SOURCE_DIR}/test/TISP_idat_gen/${KERNEL_NAME}/gen_data.py
      ${CMAKE_SOURCE_DIR}/test/TISP_idat_gen/${KERNEL_NAME}/test_cases_list.csv
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/test/TISP_idat_gen/${KERNEL_NAME}")

  add_custom_target(
    gen_test_case_headers
    DEPENDS "${CMAKE_SOURCE_DIR}/test/${KERNEL_NAME}/${kernelName}_idat.c"
            "${CMAKE_SOURCE_DIR}/test/${KERNEL_NAME}/${kernelName}_idat.cpp")

endfunction()

add_custom_target(
  gen_all_test_case_headers
  COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${_idat_pythonpath}
          ${CMAKE_SOURCE_DIR}/cmake/gen_all_test_case_headers.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/)
