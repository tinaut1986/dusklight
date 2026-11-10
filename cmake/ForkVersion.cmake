# Fork versioning (tinaut1986/dusklight).
#
# This fork has two version numbers:
#   - its own release number, which the updater compares. Release tags look like
#     v1.0.0+base.2.0.3: the "+base..." part is semver build metadata, so it is
#     ignored when comparing versions but stays visible in the tag and the UI.
#   - the upstream TwilitRealm/dusklight version it is built on ("base").
#
# The base version is the nearest upstream tag (vX.Y.Z, no "+") reachable from
# HEAD. It is exposed to C++ as DUSK_BASE_VERSION.
#
# Must be included after borealis_detect_version() so GIT_EXECUTABLE is known.

set(DUSK_BASE_VERSION "" CACHE STRING "Override the upstream base version (e.g. v2.0.3)")
# Set by the release workflow for builds published as prereleases: they always
# look for beta updates, like ftpd/NSA beta builds.
option(DUSK_BETA_BUILD "This build is published on the beta channel" OFF)

if (NOT DUSK_BASE_VERSION)
    set(DUSK_BASE_VERSION "unknown")
    if (GIT_EXECUTABLE)
        execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
                        --match "v[0-9]*.[0-9]*.[0-9]*" --exclude "*+*"
                OUTPUT_VARIABLE _fork_base
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _fork_base_result)
        if (_fork_base_result EQUAL 0 AND _fork_base)
            set(DUSK_BASE_VERSION "${_fork_base}")
        endif ()
    endif ()
endif ()

message(STATUS "fork: version ${BOREALIS_APP_DESCRIBE}, upstream base ${DUSK_BASE_VERSION}")
