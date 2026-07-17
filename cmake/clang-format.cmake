# additional target to perform clang-format run, requires clang-format

# get all project files
# file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.h)
file(GLOB_RECURSE ALL_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.h
                                   ${CMAKE_SOURCE_DIR}/examples/*.cpp ${CMAKE_SOURCE_DIR}/examples/*.h
				   ${CMAKE_SOURCE_DIR}/test/*.cpp ${CMAKE_SOURCE_DIR}/test/*.h)


## exclude auto-generated test case header files
file(GLOB_RECURSE TEST_INCLUDES  "${CMAKE_SOURCE_DIR}/test/*/test_data/*.h")
list(REMOVE_ITEM ALL_SOURCE_FILES ${TEST_INCLUDES})

add_custom_target(clangformat COMMAND /usr/bin/clang-format -style=file -i
                                      ${ALL_SOURCE_FILES})
