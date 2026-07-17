# ##############################################################################
# Macro to locate a pre-built xlib and configure its paths.
# Probes three locations in priority order:
#   1. West workspace:  ${TISP_WORKSPACE}/mcu_plus_sdk/source/${name}/
#   2. Flat sibling:    ${TISP_WORKSPACE}/${name}/
#   3. MCU_PLUS_SDK_ROOT: $ENV{MCU_PLUS_SDK_ROOT}/source/${name}/
# FATAL_ERROR (showing all three paths) if none found.
#
# Usage: tisp_configure_xlib(<NAME>
#          [HAS_COMMON]
#          [EXTRA_INCLUDES dir1 ...]
#          [PC_EXTRA_INCLUDES dir1 ...])
# ##############################################################################
macro(tisp_configure_xlib _name)
  cmake_parse_arguments(_xlib "HAS_COMMON" "" "EXTRA_INCLUDES;PC_EXTRA_INCLUDES" ${ARGN})

  string(TOLOWER ${_name} _xlib_lower)

  # Resolve xlib root: West → flat sibling → MCU_PLUS_SDK_ROOT
  set(_candidate_west    "${TISP_WORKSPACE}/mcu_plus_sdk/source/${_xlib_lower}")
  set(_candidate_sibling "${TISP_WORKSPACE}/${_xlib_lower}")
  set(_candidate_sdk     "$ENV{MCU_PLUS_SDK_ROOT}/source/${_xlib_lower}")

  if(EXISTS "${_candidate_west}/CMakeLists.txt")
    set(_xlib_root "${_candidate_west}")
  elseif(EXISTS "${_candidate_sibling}/CMakeLists.txt")
    set(_xlib_root "${_candidate_sibling}")
  elseif(NOT "$ENV{MCU_PLUS_SDK_ROOT}" STREQUAL "" AND EXISTS "${_candidate_sdk}")
    set(_xlib_root "${_candidate_sdk}")
  else()
    message(FATAL_ERROR
      "${_name} not found. Tried:\n"
      "  West:    ${_candidate_west}\n"
      "  Sibling: ${_candidate_sibling}\n"
      "  SDK:     ${_candidate_sdk} (MCU_PLUS_SDK_ROOT=$ENV{MCU_PLUS_SDK_ROOT})\n"
      "Run: west build-tisp --soc <soc>  or  export MCU_PLUS_SDK_ROOT=/path/to/sdk")
  endif()

  set(_xlib_lib_dir "${_xlib_root}/lib/${CMAKE_BUILD_TYPE}")

  if(TARGET_PLATFORM STREQUAL "PC")
    if(CMAKE_HOST_WIN32)
      set(_xlib_artifact "${_xlib_lib_dir}/${_name}_${DEVICE}_x86_64.lib")
    else()
      set(_xlib_artifact "${_xlib_lib_dir}/${_name}_${DEVICE}_x86_64.a")
    endif()
    if(NOT EXISTS "${_xlib_artifact}")
      message(FATAL_ERROR
        "${_name} artifact not found: ${_xlib_artifact}\n"
        "Run: west build-tisp --soc <soc> to build xlib dependencies first.")
    endif()
    set(${_name}_LIBS "${_xlib_artifact}")

    if(_xlib_HAS_COMMON)
      if(CMAKE_HOST_WIN32)
        set(_xlib_common_artifact "${_xlib_lib_dir}/${_name}_common_${DEVICE}_x86_64.lib")
      else()
        set(_xlib_common_artifact "${_xlib_lib_dir}/${_name}_common_${DEVICE}_x86_64.a")
      endif()
      if(NOT EXISTS "${_xlib_common_artifact}")
        message(FATAL_ERROR
          "${_name}_common artifact not found: ${_xlib_common_artifact}\n"
          "Run: west build-tisp --soc <soc> to build xlib dependencies first.")
      endif()
      list(APPEND ${_name}_LIBS "${_xlib_common_artifact}")
    endif()
  else()
    set(_xlib_artifact "${_xlib_lib_dir}/${_name}_${DEVICE}.lib")
    if(NOT EXISTS "${_xlib_artifact}")
      message(FATAL_ERROR
        "${_name} artifact not found: ${_xlib_artifact}\n"
        "Run: west build-tisp --soc <soc> --platform target to build xlib dependencies first.")
    endif()
    set(${_name}_LIBS "${_xlib_artifact}")

    if(_xlib_HAS_COMMON)
      set(_xlib_common_artifact "${_xlib_lib_dir}/${_name}_common_${DEVICE}.lib")
      if(NOT EXISTS "${_xlib_common_artifact}")
        message(FATAL_ERROR
          "${_name}_common artifact not found: ${_xlib_common_artifact}\n"
          "Run: west build-tisp --soc <soc> --platform target to build xlib dependencies first.")
      endif()
      list(PREPEND ${_name}_LIBS "${_xlib_common_artifact}")
    endif()
  endif()

  set(${_name}_INCLUDES "${_xlib_root}/src/")
  if(TARGET_PLATFORM STREQUAL "PC")
    foreach(_dir IN LISTS _xlib_PC_EXTRA_INCLUDES)
      list(APPEND ${_name}_INCLUDES "${_xlib_root}/src/${_dir}")
    endforeach()
  endif()
  foreach(_dir IN LISTS _xlib_EXTRA_INCLUDES)
    list(APPEND ${_name}_INCLUDES "${_xlib_root}/src/${_dir}")
  endforeach()
endmacro()

# ##############################################################################
# Function to change CMake's default lib prefix and extensions ##
# ##############################################################################

function(TISP_set_lib_extension LIB_NAME)
  if(TARGET_PLATFORM STREQUAL "PC")
    if(CMAKE_HOST_WIN32)
      set_target_properties(${LIB_NAME} PROPERTIES PREFIX "")
      set_target_properties(${LIB_NAME} PROPERTIES SUFFIX
                                                   "_${DEVICE}_x86_64.lib")
    else()
      set_target_properties(${LIB_NAME} PROPERTIES PREFIX "")
      set_target_properties(${LIB_NAME} PROPERTIES SUFFIX "_${DEVICE}_x86_64.a")
    endif()

  else()
    set_target_properties(${LIB_NAME} PROPERTIES PREFIX "")
    set_target_properties(${LIB_NAME} PROPERTIES SUFFIX "_${DEVICE}.lib")

    # set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> ru2 <TARGET> <LINK_FLAGS>
    # <OBJECTS>") set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> ru2  <TARGET>
    # <LINK_FLAGS> <OBJECTS>")

  endif()
endfunction()

# ##############################################################################
# Function to to add all sources in a given kernel directory and create an
# object library ##
# ##############################################################################

function(TISP_create_object_lib_for_kernel)

  string(TOLOWER ${DEVICE} DEVICE_DIR)
  if(${DEVICE} STREQUAL "C7504")
    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/${DEVICE_DIR})
      file(GLOB KERNEL_C_FILES "*.c" "c7504/*.c")
      file(GLOB KERNEL_CPP_FILES "*.cpp" "c7504/*.cpp")
      file(GLOB KERNEL_H_FILES "*.h" "c7504/*.h")
    else()
      file(GLOB KERNEL_C_FILES "*.c" "c71/*.c")
      file(GLOB KERNEL_CPP_FILES "*.cpp" "c71/*.cpp")
      file(GLOB KERNEL_H_FILES "*.h" "c71/*.h")
    endif()
  else()
    file(GLOB KERNEL_C_FILES "*.c" "c71/*.c")
    file(GLOB KERNEL_CPP_FILES "*.cpp" "c71/*.cpp")
    file(GLOB KERNEL_H_FILES "*.h" "c71/*.h")
  endif()

  if(TARGET_PLATFORM STREQUAL "PC")
    set_source_files_properties(${KERNEL_C_FILES} PROPERTIES LANGUAGE CXX)
  endif()

  list(APPEND KERNEL_FILES ${KERNEL_CPP_FILES} ${KERNEL_C_FILES}
       ${KERNEL_H_FILES})

  get_filename_component(myKernelName "${CMAKE_CURRENT_LIST_DIR}" NAME)
  add_library("${myKernelName}_obj" OBJECT ${KERNEL_FILES})
  target_include_directories(
    "${myKernelName}_obj"
    PUBLIC "${CMAKE_SOURCE_DIR}/src/common" . "${CMAKE_SOURCE_DIR}/src/"
           ${C7x_INCLUDES} ${C7X_UDMA_ROOT})

  if(${TISP_USE_INTERNEL} STREQUAL "True")
    add_dependencies("${myKernelName}_obj" ${TISP_DEPENDENCIES})
  endif()

endfunction()

# ##############################################################################
# Function to print build details ##
# ##############################################################################

function(TISP_print_build_details)
  if(TARGET_PLATFORM STREQUAL "PC")
    message(STATUS "                                          ")
    message(STATUS "${BoldYellow}------------------------------------------")
    message(STATUS "${BoldYellow}   Building for Host Emulation(x86_64)    ")
    message(STATUS "${BoldYellow}------------------------------------------")
    message(STATUS "${BoldYellow}   DEVICE: ${DEVICE}                      ")
    message(STATUS "                                          ")

  else()
    message(STATUS "                                          ")
    message(STATUS "${BoldBlue}------------------------------------------")
    message(STATUS "${BoldBlue}   Building for Target                    ")
    message(STATUS "${BoldBlue}------------------------------------------")
    message(STATUS "${BoldBlue}   DEVICE: ${DEVICE}                      ")
    message(STATUS "                                          ")
  endif()

endfunction()

# TODO: scoping exeName
function(TISP_finish_compilation exeName)

  if(TARGET_PLATFORM STREQUAL "PC")
    set(myColor "yellow")
  else()
    set(myColor "blue")
  endif()

  if(AUTO_TEST)

  else()
    add_custom_target(
      FinalMessage ALL
      COMMAND ${CMAKE_COMMAND} -E cmake_echo_color
      --${myColor}
      --bold
      --mma_version=$(MMA)
      "Compilation is over for ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/${exeName}"
      # COMMENT "Final Message"
    )

    add_dependencies(FinalMessage ${exeName})
  endif()

endfunction()

set(TISP_C7x_linker_SOC_defines
    --define=j721e=1
    --define=j721s2=2
    --define=j784s4=3
    --define=AM62A=4
    --define=j722s=5
    --define=AM62D=6
    --define=AM275=7)
set(TISP_C7x_SOC_defines
    -D=j721e=1
    -D=j721s2=2
    -D=j784s4=3
    -D=AM62A=4
    -D=AM62D=5
    -D=j722s=6
    -D=AM275=7)
set(TISP_x86_SOC_defines
    j721e=1
    j721s2=2
    j784s4=3
    AM62A=4
    AM62D=5
    j722s=6
    AM275=7)

# how do I do add a long list without using concat set(foo
# --display_error_number --diag_remark=10068 --abi=eabi) set(foo ${foo}
# --opt_level=3 --gen_opt_info=2 ) set(foo ${foo} --symdebug:none -os -mw
# --disable:NLC) set(foo ${foo} --auto_inline -mo=on --preproc_with_compile)
# set(TISP_C7x_compile_release_options ${foo})

function(TISP_compile_options)
  string(TOUPPER ${SOC} SOC_CAPS)

  # SOC-specific defines: <SOC_FULL_X> <SOC_SHORT> <evm_name=evm_name>
  set(_SOC_DEFS_AM62A  SOC_AM62AX  SOC_AM62A  am62a_evm=am62a_evm)
  set(_SOC_DEFS_AM62D  SOC_AM62DX  SOC_AM62DX am62d_evm=am62d_evm)
  set(_SOC_DEFS_AM275  SOC_AM275X  SOC_AM275X am275_evm=am275_evm)
  set(_SOC_DEFS_j722s  SOC_J722SX  SOC_J722S  j722s_evm=j722s_evm)
  set(_SOC_COMMON_DEFS MCU_PLUS_SDK BUILD_C7X_1 BUILD_C7X DMA_UTILS_STANDALONE)

  if(TARGET_PLATFORM STREQUAL "PC")

    add_compile_definitions(${_SOC_DEFS_${SOC}} ${_SOC_COMMON_DEFS})
    add_compile_definitions(_HOST_BUILD)
    add_compile_definitions(HOST_EMULATION)
    add_compile_definitions(SOC_${SOC_CAPS})
    add_compile_definitions(__${DEVICE}__)
    add_compile_definitions(SOC=${SOC})

    add_compile_definitions(${TISP_x86_SOC_defines})
    add_compile_definitions(TISP_DEBUGPRINT=${TISP_DEBUGPRINT})
    add_compile_definitions(C7X)
    # add_compile_definitions(DEVM_TEST=${EVM_TEST})
    if(CMAKE_HOST_WIN32)
      add_compile_definitions(WIN32)
    else()

      add_compile_options(
        # --std=c++14
        -coverage
        -fPIC
        -Wall
        -g
        # -Wextra -Wconversion -Wsign-conversion
        -Wno-int-to-pointer-cast
        -Wno-unused-but-set-variable
        -Wno-reorder
        -Wno-unused-variable
        -Wno-format
        -Wno-pointer-arith
        -fms-extensions
        -Wno-write-strings
        -Wno-unknown-pragmas
        -fno-strict-aliasing
        $<$<CONFIG:DEBUG>:-g3>
        $<$<CONFIG:DEBUG>:-O0>
        $<$<CONFIG:RELEASE>:-O3>)
      if(MEM_CHECK)
        add_compile_options(-fsanitize=address)
        add_compile_options(-fsanitize-recover=address)
        add_link_options(-fsanitize=address)
      endif()
    endif()

    # add_compile_definitions ( TEST_CASE=${TEST_CASE} )

  else()
    string(REGEX REPLACE "^C" "" siliconVersion ${DEVICE})

    set(_c7x_soc_opts "")
    foreach(_def IN LISTS _SOC_DEFS_${SOC} _SOC_COMMON_DEFS)
      list(APPEND _c7x_soc_opts "-D${_def}")
    endforeach()
    add_compile_options(${_c7x_soc_opts})
    add_compile_options(
      --silicon_version=${siliconVersion}
      -q
      -mo
      -pden
      -pds=238
      -pds=880
      -pds1110
      --disable_inlining
      # --emit_warnings_as_errors
      --diag_suppress=770
      --diag_suppress=69
      --diag_suppress=70
      --diag_suppress=35000
      -DBUILD_C7X
      -DBUILD_C7X_1
      -DC7X
      # ########################################################################
      -DSOC_${SOC_CAPS}
      --silicon_version=${siliconVersion}
      ${TISP_C7x_SOC_defines}
      -DTISP_DEBUGPRINT=${TISP_DEBUGPRINT}
      -DEVM_TEST
      -DSOC=${SOC}
      -DDISABLE_STACK_PROFILING
      --c++14
      -os
      -q
      -mo
      -pden
      -pds=238
      -pds=880
      -pds110
      --endian=little
      # -eo.oe71 -ea.se71 --silicon_errata_i2117
      -Dxdc_target_name__=C71
      -D_DEBUG_=1
      # -Dxdc_target_types__=ti/targets/elf/std.h
      #--symdebug:none
      -DTISP_LOKI_BUILD
      # Need to figure out a way to pass var here
      # $<$<CONFIG:RELEASE>:${TISP_C7x_compile_release_options}>
      $<$<CONFIG:RELEASE>:--opt_level=3>
      $<$<CONFIG:RELEASE>:--display_error_number>
      $<$<CONFIG:RELEASE>:--diag_remark=10068>
      $<$<CONFIG:RELEASE>:--abi=eabi>
      $<$<CONFIG:RELEASE>:--gen_opt_info=2>
      $<$<CONFIG:RELEASE>:-os>
      $<$<CONFIG:RELEASE>:-mw>
      $<$<CONFIG:RELEASE>:--symdebug:none>
      # $<$<CONFIG:RELEASE>:--disable:NLC>
      $<$<CONFIG:RELEASE>:--auto_inline>
      $<$<CONFIG:RELEASE>:-mo=on>
      $<$<CONFIG:RELEASE>:--preproc_with_compile>
      $<$<CONFIG:DEBUG>:--opt_level=0>
      $<$<CONFIG:DEBUG>:--disable_inlining>
      $<$<CONFIG:DEBUG>:-g>
    )
  endif()

endfunction()

function(TISP_set_linker_options_C7x exeName)

  string(REGEX REPLACE "^C" "" siliconVersion ${DEVICE})
  set(myLinkerFile ${CMAKE_SOURCE_DIR}/cmake/linkers/${DEVICE}/lnk.cmd)
  set(myLinkerCmd
      # --silicon_version=${siliconVersion}
      --library=${myLinkerFile}
      --search_path=$ENV{CGT7X_ROOT}/lib
      --reread_libs
      --ram_model
      --warn_sections
      -q
      -x
      --emit_warnings_as_errors
      --diag_suppress=10063)

  target_link_options(${exeName} BEFORE PRIVATE --define=SOC=${SOC})

  target_link_options(${exeName} PRIVATE ${TISP_C7x_linker_SOC_defines}
                      ${myLinkerCmd})
endfunction()

function(tisp_include_xlib_headers)

  include_directories(BEFORE SYSTEM src/)

endfunction()

function(tisp_set_bin_extension exeNAME)
  if(TARGET_PLATFORM STREQUAL "PC")
    if(CMAKE_HOST_WIN32)
      set_target_properties(${exeNAME} PROPERTIES PREFIX "")
      set_target_properties(${exeNAME} PROPERTIES SUFFIX ".exe")
    else()
      set_target_properties(${exeNAME} PROPERTIES PREFIX "")
      set_target_properties(${exeNAME} PROPERTIES SUFFIX "")
    endif()

  else()
    set_target_properties(${exeNAME} PROPERTIES PREFIX "")
    set_target_properties(${exeNAME} PROPERTIES SUFFIX ".out")

    # set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> ru2 <TARGET> <LINK_FLAGS>
    # <OBJECTS>") set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> ru2  <TARGET>
    # <LINK_FLAGS> <OBJECTS>")

  endif()
endfunction()

function(tisp_fetch_extProj_config libName)

  # Read the JSON configuration file
  set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/extProjConfig.json")
  if(NOT EXISTS "${CONFIG_FILE}")
    message(FATAL_ERROR "Repository config file not found: ${CONFIG_FILE}")
  endif()

  file(READ "${CONFIG_FILE}" CONFIG_JSON)

  # Extract URL and tag
  string(JSON url GET ${CONFIG_JSON} ${libName} "url")
  string(JSON tag GET ${CONFIG_JSON} ${libName} "tag")

  # Print the info
  message(STATUS "Library: ${libName}")
  message(STATUS "  URL: ${url}")
  message(STATUS "  Tag: ${tag}")

  # pass variables to parent scope
  set(${libName}_URL
      ${url}
      PARENT_SCOPE)
  set(${libName}_TAG
      ${tag}
      PARENT_SCOPE)

endfunction()

function(tisp_add_extProj_dependencies libName installName)

  # lower libName
  string(TOLOWER ${libName} libNameLower)

  # upper libName
  string(TOUPPER ${libName} libNameUpper)

  # fetch lib content from config file
  tisp_fetch_extproj_config(${libNameLower})

  # cmake command
  list(
    APPEND
    xlibCmakeConfigArgs
    "-DSOC=${SOC}"
    "-DDEVICE=${DEVICE}"
    "-DMMA=${MMA}"
    "-DBUILD_LIB=1"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-D${libNameUpper}_DEBUGPRINT=0"
    "-DTARGET_PLATFORM=${TARGET_PLATFORM}"
    )

  ExternalProject_Add(
    ${libNameLower}
    GIT_REPOSITORY ${${libNameLower}_URL}
    GIT_TAG ${${libNameLower}_TAG}
    GIT_SUBMODULES ""
    GIT_PROGRESS   OFF
    LOG_DOWNLOAD   ON   
    LOG_OUTPUT_ON_FAILURE ON
    LOG_UPDATE ON
    LOG_MERGED_STDOUTERR ON   # Merges error and standard output into one log
    DOWNLOAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${installName}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${installName}/${libNameLower}
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/${installName}/${libNameLower}
    CMAKE_ARGS   ${xlibCmakeConfigArgs} 
                 -DCMAKE_MESSAGE_LOG_LEVEL:STRING=WARNING # <--- Silences sub-cmake
    INSTALL_COMMAND ""   
  )

  # FetchContent_Declare( ${libNameLower} GIT_REPOSITORY ${${libNameLower}_URL}
  # GIT_TAG ${${libNameLower}_TAG}

  # GIT_SUBMODULES ""

  # CMAKE_ARGS ${xlibCmakeConfigArgs} )

  # FetchContent_MakeAvailable(dsplib)

endfunction()

function(tisp_add_extProj_dependencies_fft_math libName installName)

  # lower libName
  string(TOLOWER ${libName} libNameLower)

  # upper libName
  string(TOUPPER ${libName} libNameUpper)

  # fetch lib content from config file
  tisp_fetch_extproj_config(${libNameLower})

  # cmake command
  list(
    APPEND
    xlibCmakeConfigArgs
    "-DSOC=${SOC}"
    "-DDEVICE=${DEVICE}"
    "-DBUILD_LIB=1"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-D${libNameUpper}_DEBUGPRINT=0"
    "-DTARGET_PLATFORM=${TARGET_PLATFORM}"
    )

  ExternalProject_Add(
    ${libNameLower}
    GIT_REPOSITORY ${${libNameLower}_URL}
    GIT_TAG ${${libNameLower}_TAG}
    GIT_SUBMODULES ""
    GIT_PROGRESS   OFF
    LOG_DOWNLOAD   ON   
    LOG_OUTPUT_ON_FAILURE ON 
    LOG_UPDATE ON
    LOG_MERGED_STDOUTERR ON   # Merges error and standard output into one log
    DOWNLOAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${installName}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${installName}/${libNameLower}
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/${installName}/${libNameLower}
    CMAKE_ARGS   ${xlibCmakeConfigArgs} 
                 -DCMAKE_MESSAGE_LOG_LEVEL:STRING=WARNING # <--- Silences sub-cmake
    INSTALL_COMMAND ""   
  )
  # FetchContent_Declare( ${libNameLower} GIT_REPOSITORY ${${libNameLower}_URL}
  # GIT_TAG ${${libNameLower}_TAG}

  # GIT_SUBMODULES ""

  # CMAKE_ARGS ${xlibCmakeConfigArgs} )

  # FetchContent_MakeAvailable(dsplib)

endfunction()

function(tisp_add_mcu_plus_sdk_dependencies)

  # Read the JSON configuration file
  set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/extProjConfig.json")
  if(NOT EXISTS "${CONFIG_FILE}")
    message(FATAL_ERROR "Repository config file not found: ${CONFIG_FILE}")
  endif()

  file(READ "${CONFIG_FILE}" CONFIG_JSON)

  set(sdkName "mcu_plus_sdk_${SOC}")
  # Extract URL
  string(JSON INSTALLER_URL GET ${CONFIG_JSON} ${sdkName} "url")
  string(JSON INSTALLER_FILENAME GET ${CONFIG_JSON} ${sdkName} "fileName")
  set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

  set(INSTALLER_FULL_PATH
      "${INSTALL_DIR}/${INSTALLER_FILENAME}-linux-x64-installer.run")

  # Define the installation check file
  set(INSTALLATION_CHECK_FILE
      "${INSTALL_DIR}/${INSTALLER_FILENAME}/.mcu_plus_sdk_installed")

  # Check if the installer file exists and if installation has completed
  if(NOT EXISTS "${INSTALLER_FULL_PATH}" OR NOT EXISTS
                                            "${INSTALLATION_CHECK_FILE}")
    message(
      STATUS
        "Installer file does not exist or installation is incomplete. Downloading..."
    )
  set(DINSTALLER_LOG ${CMAKE_CURRENT_BINARY_DIR}/mcu_sdk/mcusdk-console.log)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mcu_sdk)

    # Define a custom target for installing the SDK (named mcu_plus_sdk)
    add_custom_target(
      mcu_plus_sdk
      COMMAND ${CMAKE_COMMAND} -E echo "Running Installer, logging to ${DINSTALLER_LOG}"
      COMMAND /bin/bash -c
              "wget --no-proxy ${INSTALLER_URL} -O ${INSTALLER_FULL_PATH}"  > "${DINSTALLER_LOG}" 2>&1
      COMMAND /bin/bash -c "chmod +x ${INSTALLER_FULL_PATH}"
      COMMAND /bin/bash -c
              "${INSTALLER_FULL_PATH} --prefix ${INSTALL_DIR} --mode unattended" > "${DINSTALLER_LOG}" 2>&1
      COMMAND /bin/bash -c "touch ${INSTALLATION_CHECK_FILE}" > "${DINSTALLER_LOG}" 2>&1
      # WORKING_DIRECTORY ${CMAKE_BINARY_DIR} # Optional: Set working directory
    )
  else()
    message(
      STATUS
        "Installer file already exists and installation is complete. Skipping download and install."
    )
    # Create a dummy target to be the dependency
    add_custom_target(mcu_plus_sdk) # Do nothing; just create a target.
  endif()

endfunction()
