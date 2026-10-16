# Runs in script mode on every build so the hash tracks HEAD without a reconfigure.
# Expects HOT_SOURCE_DIR, HOT_TEMPLATE, HOT_OUTPUT and optionally HOT_COMMIT_OVERRIDE.

set(HOT_COMMIT_HASH "0000000000000000000000000000000000000000")
set(HOT_COMMIT_DIRTY 0)

string(LENGTH "${HOT_COMMIT_OVERRIDE}" _override_length)

if(_override_length EQUAL 40 AND HOT_COMMIT_OVERRIDE MATCHES "^[0-9a-fA-F]+$")
    string(TOLOWER "${HOT_COMMIT_OVERRIDE}" HOT_COMMIT_HASH)
else()
    find_package(Git QUIET)

    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
            WORKING_DIRECTORY "${HOT_SOURCE_DIR}"
            OUTPUT_VARIABLE _hash
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _hash_result
        )
        string(LENGTH "${_hash}" _hash_length)

        if(_hash_result EQUAL 0 AND _hash_length EQUAL 40 AND _hash MATCHES "^[0-9a-fA-F]+$")
            string(TOLOWER "${_hash}" HOT_COMMIT_HASH)
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
            WORKING_DIRECTORY "${HOT_SOURCE_DIR}"
            OUTPUT_VARIABLE _dirty
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(NOT _dirty STREQUAL "")
            set(HOT_COMMIT_DIRTY 1)
        endif()
    endif()
endif()

# configure_file only rewrites the output when the content changes, so this does not force rebuilds.
configure_file("${HOT_TEMPLATE}" "${HOT_OUTPUT}" @ONLY)
