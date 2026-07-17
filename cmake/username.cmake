# Resolves the current Linux username in a robust way.
# Usage: resolve_linux_username(<OUT_VAR>)
# - <OUT_VAR> is the name of the variable that will receive the username string.
function(resolve_linux_username OUT_VAR)
  # --------------------------
  # 1) Parameter validation
  # --------------------------
  if (NOT ARGC EQUAL 1)
    message(FATAL_ERROR
      "resolve_linux_username: expected exactly 1 parameter (OUT_VAR); got ${ARGC}.")
  endif()

  if ("${OUT_VAR}" STREQUAL "")
    message(FATAL_ERROR
      "resolve_linux_username: OUT_VAR cannot be empty.")
  endif()

  # CMake variable names should be letters/digits/underscore (avoid whitespace/special chars)
  if ("${OUT_VAR}" MATCHES "^[ \t]+$" OR "${OUT_VAR}" MATCHES "[^A-Za-z0-9_]")
    message(FATAL_ERROR
      "resolve_linux_username: OUT_VAR must be a valid CMake variable name (letters, digits, underscore).")
  endif()

  # --------------------------
  # 2) Username resolution
  #    (preserves your original precedence and logic)
  #    - Manual override
  #    - Common env vars (SUDO_USER, USER, LOGNAME)
  #    - Fallback to `whoami`
  # --------------------------
  set(OVERRIDE_USERNAME "${OVERRIDE_USERNAME}" CACHE STRING "Override detected username")  # allow CI overrides
  if (OVERRIDE_USERNAME)
    set(_USERNAME "${OVERRIDE_USERNAME}")
  else()
    # Prefer the non-root invoker when running under sudo
    set(_USERNAME "$ENV{SUDO_USER}")

    # Common interactive env vars
    if (NOT _USERNAME)
      set(_USERNAME "$ENV{USER}")
    endif()
    if (NOT _USERNAME)
      set(_USERNAME "$ENV{LOGNAME}")
    endif()

    # Fallback to system command
    if (NOT _USERNAME)
      execute_process(
        COMMAND whoami
        OUTPUT_VARIABLE _USERNAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
  endif()
  # (The above mirrors your uploaded logic and ordering.) 

  # --------------------------
  # 3) Postcondition & output
  # --------------------------
  # Always set the output variable in the caller's scope
  set(${OUT_VAR} "${_USERNAME}" PARENT_SCOPE)

  # Enforce non-empty, non-whitespace username
  if (NOT DEFINED _USERNAME OR _USERNAME MATCHES "^[ \t]*$")
    message(FATAL_ERROR
      "resolve_linux_username: unable to resolve current username. "
      "Consider setting -DOVERRIDE_USERNAME=<user> or ensuring USER/LOGNAME are set.")
  endif()
endfunction()

