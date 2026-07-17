# CppUTest
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v2.13.10 # or use release tag, eg. v4.0
)
# Set this to ON if you want to have the CppUTests in your project as well.
set(TESTS
    OFF
    CACHE BOOL "Switch off CppUTest Test build")
FetchContent_MakeAvailable(Catch2)
