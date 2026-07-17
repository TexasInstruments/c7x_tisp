# file(GLOB_RECURSE COMMON_FILES "*.cpp" "*.c")

string(TOLOWER ${DEVICE} DEVICE_DIR)
file(GLOB COMMON_C_FILES "*.c" "c71/*.c" "${DEVICE_DIR}/*.c")
file(GLOB COMMON_CPP_FILES "*.cpp" "c71/*.cpp" "${DEVICE_DIR}/*.cpp")
file(GLOB COMMON_H_FILES "*.h" "c71/*.h" "${DEVICE_DIR}/*.h")

if(TARGET_PLATFORM STREQUAL "PC")
  set_source_files_properties(${COMMON_C_FILES} PROPERTIES LANGUAGE CXX)
endif()

list(APPEND COMMON_FILES ${COMMON_CPP_FILES} ${COMMON_C_FILES}
     ${COMMON_H_FILES})

add_library(TISP_common STATIC ${COMMON_FILES})

if(${TISP_USE_INTERNEL} STREQUAL "True")
  add_dependencies(TISP_common ${TISP_DEPENDENCIES})
endif()

tisp_set_lib_extension(TISP_common)

target_include_directories(
  TISP_common PUBLIC ../../src/common . ${CMAKE_SOURCE_DIR}/includes
                     ${C7X_INCLUDES} ${C7X_UDMA_ROOT})
