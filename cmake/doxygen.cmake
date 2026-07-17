# Doxygen

# look for Doxygen package
find_package(Doxygen)
# find_package(MathJax)

if(DOXYGEN_FOUND)
  # set input and output files
  set(DOXYGEN_USER_GUIDE_IN
      ${CMAKE_SOURCE_DIR}/docs/doxygen/doxyfile_user_guide)
  set(DOXYGEN_USER_GUIDE_OUT
      ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_user_guide.out)

  set(DOXYGEN_DATASHEET_IN ${CMAKE_SOURCE_DIR}/docs/doxygen/doxyfile_datasheet)
  set(DOXYGEN_DATASHEET_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_datasheet.out)

  # request to configure the file
  configure_file(${DOXYGEN_USER_GUIDE_IN} ${DOXYGEN_USER_GUIDE_OUT} @ONLY)
  message(STATUS "Doxygen build started")

  # request to configure the file
  configure_file(${DOXYGEN_DATASHEET_IN} ${DOXYGEN_DATASHEET_OUT} @ONLY)
  message(STATUS "Doxygen build started")

  # bad hack
  file(READ ${DOXYGEN_USER_GUIDE_OUT} FILE_CONTENTS)
  string(REPLACE "./docs" "../docs" FILE_CONTENTS "${FILE_CONTENTS}")
  file(WRITE ${DOXYGEN_USER_GUIDE_OUT} "${FILE_CONTENTS}")

  # bad hack
  file(READ ${DOXYGEN_DATASHEET_OUT} FILE_CONTENTS)
  string(REPLACE "./docs" "../docs" FILE_CONTENTS "${FILE_CONTENTS}")
  file(WRITE ${DOXYGEN_DATASHEET_OUT} "${FILE_CONTENTS}")
  
  set(DOXYGEN_LOG ${CMAKE_CURRENT_BINARY_DIR}/doxygen/doxygen-console.log)
  set(DATASHEET_LOG ${CMAKE_CURRENT_BINARY_DIR}/doxygen/datasheet-console.log)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen)

  # Note: do not put "ALL" - this builds docs together with application EVERY
  # TIME!
  add_custom_target(
    user_guide
    COMMAND ${CMAKE_COMMAND} -E echo "Running Doxygen user_guide, logging to ${DOXYGEN_LOG}"
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_USER_GUIDE_OUT} > "${DOXYGEN_LOG}" 2>&1
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)

  add_custom_target(
    datasheet
    COMMAND ${CMAKE_COMMAND} -E echo "Running Doxygen datasheet, logging to ${DATASHEET_LOG}"
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_DATASHEET_OUT} > "${DATASHEET_LOG}" 2>&1
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)
else(DOXYGEN_FOUND)
  message(STATUS "Doxygen need to be installed to generate the doxygen documentation")
endif(DOXYGEN_FOUND)
