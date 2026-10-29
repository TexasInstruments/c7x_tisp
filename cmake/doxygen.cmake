# Doxygen + Sphinx documentation build
#
# Targets:
#   user_guide        -- Doxygen XML -> sphinx-build -> TI-branded HTML
#   user_guide_doxygen -- legacy Doxygen-only HTML (unchanged from original)
#   datasheet          -- Doxygen HTML datasheet (unchanged from original)

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
    user_guide_doxygen
    COMMAND ${CMAKE_COMMAND} -E echo "Running Doxygen in user guide, logging to ${DOXYGEN_LOG}"
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_USER_GUIDE_OUT} > "${DOXYGEN_LOG}" 2>&1
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)

  add_custom_target(
    datasheet
    COMMAND ${CMAKE_COMMAND} -E echo "Running Doxygen data sheet, logging to ${DATASHEET_LOG}"
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_DATASHEET_OUT} > "${DATASHEET_LOG}" 2>&1
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)

endif(DOXYGEN_FOUND)

# ------------------------------------------------------------------
# Sphinx user_guide target (Doxygen XML -> sphinx-build -> HTML)
# ------------------------------------------------------------------
# look for sphinx-build
find_program(SPHINX_BUILD_EXECUTABLE NAMES sphinx-build)

if(DOXYGEN_FOUND AND SPHINX_BUILD_EXECUTABLE)

  # ------------------------------------------------------------------
  # CMake variables substituted into doxyfile_sphinx via configure_file
  #
  #   DOXYGEN_XML_OUT_PARENT  -- parent of the xml/ subdir
  #                              => ${CMAKE_CURRENT_BINARY_DIR}/docs
  #                              => XML lands in ${CMAKE_CURRENT_BINARY_DIR}/docs/xml
  #
  #   DOXYGEN_SRC_ROOT        -- absolute source-tree root
  #                              => ${CMAKE_SOURCE_DIR}
  #
  #   DOXYGEN_SRC_DOCS        -- absolute path to source-tree docs/
  #                              => ${CMAKE_SOURCE_DIR}/docs
  # ------------------------------------------------------------------
  set(DOXYGEN_XML_OUT_PARENT "${CMAKE_CURRENT_BINARY_DIR}/docs")
  set(DOXYGEN_SRC_ROOT       "${CMAKE_SOURCE_DIR}")
  set(DOXYGEN_SRC_DOCS       "${CMAKE_SOURCE_DIR}/docs")

  set(DOXYGEN_SPHINX_IN  ${CMAKE_SOURCE_DIR}/docs/doxygen/doxyfile_sphinx)
  set(DOXYGEN_SPHINX_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.sphinx.out)

  configure_file(${DOXYGEN_SPHINX_IN} ${DOXYGEN_SPHINX_OUT} @ONLY)
  message(STATUS "Doxygen+Sphinx build configured")

  # Ensure the log directory exists at configure time so the COMMAND
  # redirect does not fail on a clean build.
  set(DOXYGEN_LOG_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
  file(MAKE_DIRECTORY ${DOXYGEN_LOG_DIR})
  set(DOXYGEN_SPHINX_LOG ${DOXYGEN_LOG_DIR}/doxygen-sphinx-console.log)

  # Sphinx HTML output directory
  set(SPHINX_HTML_OUT ${CMAKE_CURRENT_BINARY_DIR}/docs/user_guide)

  # Sphinx source directory (contains conf.py and index.rst)
  set(SPHINX_SOURCE_DIR ${CMAKE_SOURCE_DIR}/docs/sphinx)

  # Absolute XML dir passed to sphinx-build via env var TISP_DOXYGEN_XML
  set(DOXYGEN_XML_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs/xml)

  # Note: do not put "ALL" -- this builds docs together with the application
  # EVERY TIME which is undesirable for incremental builds.
  add_custom_target(
    user_guide
    COMMAND ${CMAKE_COMMAND} -E echo
            "Running Doxygen (XML), logging to ${DOXYGEN_SPHINX_LOG}"
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_SPHINX_OUT}
              > "${DOXYGEN_SPHINX_LOG}" 2>&1
    COMMAND ${CMAKE_COMMAND} -E echo
            "Running sphinx-build -> ${SPHINX_HTML_OUT}"
    COMMAND ${CMAKE_COMMAND} -E env
              TISP_DOXYGEN_XML=${DOXYGEN_XML_DIR}
              ${SPHINX_BUILD_EXECUTABLE}
              -b html
              -W --keep-going
              ${SPHINX_SOURCE_DIR}
              ${SPHINX_HTML_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating Sphinx (TI-branded) API docs"
    VERBATIM)

else()

  if(NOT DOXYGEN_FOUND)
    message(STATUS
      "Doxygen not found -- install Doxygen to enable the user_guide target")
  endif()
  if(NOT SPHINX_BUILD_EXECUTABLE)
    message(STATUS
      "sphinx-build not found -- install Sphinx to enable the user_guide target."
      "  Hint: pip install -r docs/sphinx/requirements.txt")
  endif()

endif()
