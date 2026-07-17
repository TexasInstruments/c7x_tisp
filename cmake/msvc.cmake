set(MSVC_TOOLSET_VERSION 143) # VS 2022 (17.0)
# if(CMAKE_BUILD_TYPE STREQUAL RELEASE)
#   set(C7x_LIBS "$ENV{CGT7X_ROOT}/host_emulation/${DEVICE}-host-emulation.lib")
# else()
#   set(C7x_LIBS "$ENV{CGT7X_ROOT}/host_emulation/${DEVICE}-host-emulationd.lib")
# endif()

set(C7x_LIBS "$ENV{CGT7X_ROOT}/host_emulation/${DEVICE}-host-emulation.lib")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
