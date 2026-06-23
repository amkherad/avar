# NBGV-style versioning from version.json + git height.
# Re-configures when version.json or git HEAD moves.

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(AVAR_VERSION_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/compute_version.py")
set(AVAR_VERSION_GEN_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/generate_version.py")
set(AVAR_VERSION_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
set(AVAR_VERSION_HEADER "${AVAR_VERSION_INCLUDE_DIR}/avar_version.h")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/version.json"
)
if(EXISTS "${CMAKE_SOURCE_DIR}/.git/logs/HEAD")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/.git/logs/HEAD"
    )
endif()

file(MAKE_DIRECTORY "${AVAR_VERSION_INCLUDE_DIR}")

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${AVAR_VERSION_GEN_SCRIPT}"
        --root "${CMAKE_SOURCE_DIR}"
        --out-c-header "${AVAR_VERSION_HEADER}"
    RESULT_VARIABLE _avar_version_gen_rc
    ERROR_VARIABLE _avar_version_gen_err
)
if(_avar_version_gen_rc)
    message(FATAL_ERROR "Failed to generate version header: ${_avar_version_gen_err}")
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${AVAR_VERSION_SCRIPT}"
        --root "${CMAKE_SOURCE_DIR}"
        --format cmake
    OUTPUT_VARIABLE _avar_version_cmake
    RESULT_VARIABLE _avar_version_rc
    ERROR_VARIABLE _avar_version_err
)
if(_avar_version_rc)
    message(FATAL_ERROR "Failed to compute version: ${_avar_version_err}")
endif()

file(WRITE "${CMAKE_BINARY_DIR}/generated/avar_version_vars.cmake" "${_avar_version_cmake}")
include("${CMAKE_BINARY_DIR}/generated/avar_version_vars.cmake")

message(STATUS "Avar version: ${AVAR_VERSION_STRING} (${AVAR_VERSION_COMMIT})")

function(avar_apply_version target)
    target_include_directories(${target} PRIVATE "${AVAR_VERSION_INCLUDE_DIR}")
    add_dependencies(${target} avar_generate_version)
endfunction()

add_custom_target(avar_generate_version
    COMMAND "${Python3_EXECUTABLE}" "${AVAR_VERSION_GEN_SCRIPT}"
        --root "${CMAKE_SOURCE_DIR}"
        --out-c-header "${AVAR_VERSION_HEADER}"
    BYPRODUCTS "${AVAR_VERSION_HEADER}"
    COMMENT "Generating Avar version header"
    VERBATIM
)
