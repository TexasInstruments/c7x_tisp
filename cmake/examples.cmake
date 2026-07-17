function(TISP_create_example_binary_name myKernelName)
  if(TARGET_PLATFORM STREQUAL "PC")
    set(exeName
        "example_${myKernelName}_${DEVICE}_x86_64"
        PARENT_SCOPE)
  else()
    set(exeName
        "example_${myKernelName}_${DEVICE}"
        PARENT_SCOPE)
  endif()
endfunction()

function(TISP_create_binary_for_example_kernel)

  file(GLOB_RECURSE SRC_FILES "*.cpp" "*.h")
  get_filename_component(myKernelName "${CMAKE_CURRENT_LIST_DIR}" NAME)
  tisp_create_example_binary_name(${myKernelName})
  add_executable(${exeName} ${SRC_FILES})
  tisp_set_bin_extension(${exeName})
  target_link_libraries(${exeName} PRIVATE TISP TISP_common ${C7x_LIBS})

  if(TARGET_PLATFORM STREQUAL "PC")
    if(CMAKE_HOST_UNIX)
      target_link_options(${exeName} PRIVATE -coverage -lgcov)
    endif()
  endif()

  if(NOT TARGET_PLATFORM STREQUAL "PC")
    tisp_set_linker_options_c7x(${exeName})
  endif()

  tisp_finish_compilation(${exeName})

endfunction()
