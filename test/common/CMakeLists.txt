enable_language(ASM)

string(TOLOWER ${DEVICE} DEVICE_DIR)
file(GLOB COMMON_C_FILES "*.c" "c71/*.c" "${DEVICE_DIR}/*.c")
file(GLOB COMMON_CPP_FILES "*.cpp" "c71/*.cpp" "${DEVICE_DIR}/*.cpp")
file(GLOB COMMON_H_FILES "*.h" "c71/*.h" "${DEVICE_DIR}/*.h")
file(GLOB COMMON_ASM_FILES "*.asm" "c71/*.asm" "${DEVICE_DIR}/*.asm")

file(GLOB COMMON_GENERATED_C_FILES "generated/${DEVICE_DIR}/*.c")
file(GLOB COMMON_GENERATED_H_FILES "generated/${DEVICE_DIR}/*.h")

if(TARGET_PLATFORM STREQUAL "PC")
  set_source_files_properties(${COMMON_C_FILES} PROPERTIES LANGUAGE CXX)
endif()

if(TARGET_PLATFORM STREQUAL "PC")
  list(APPEND COMMON_FILES ${COMMON_CPP_FILES} ${COMMON_C_FILES}
       ${COMMON_H_FILES})
else()
  list(
    APPEND
    COMMON_FILES
    ${COMMON_CPP_FILES}
    ${COMMON_C_FILES}
    ${COMMON_H_FILES}
    ${COMMON_ASM_FILES}
    ${COMMON_GENERATED_C_FILES}
    ${COMMON_GENERATED_H_FILES})
endif()

add_library(TISP_test_common STATIC ${COMMON_FILES})

tisp_set_lib_extension(TISP_test_common)

target_include_directories(
  TISP_test_common PUBLIC ../common ${CMAKE_SOURCE_DIR}/includes
                          ${C7X_INCLUDES} ${DMA_UTILS_INCLUDE_DIR})

if(${TISP_USE_INTERNEL} STREQUAL "True")
  add_dependencies(TISP_test_common ${TISP_DEPENDENCIES})
endif()
