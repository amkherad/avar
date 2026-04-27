cmake_minimum_required(VERSION 4.1)
project(avar C)
set(CMAKE_C_STANDARD 23)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(SOURCE_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
set(THIRD_PARTY_DIR "${SRC_DIR}/third_party")
file(MAKE_DIRECTORY "${SOURCE_GEN_DIR}")

find_package(Python3 REQUIRED)

set(DOCOPT_SOURCES_OUTPUT)
function(define_cli_command cli_command)
    # --- Resolve paths -------------------------------------------------
    set(DOCOPT_INPUT   "${SRC_DIR}/cli/${cli_command}.docopt")
    set(DOCOPT_OUTPUT  "${SOURCE_GEN_DIR}/${cli_command}_cli_handler")
    set(DOCOPT_OUTPUT_C  "${DOCOPT_OUTPUT}.c")
    set(DOCOPT_OUTPUT_H  "${DOCOPT_OUTPUT}.h")
    message(STATUS "Generating command for CLI handler from ${DOCOPT_INPUT} to ${DOCOPT_OUTPUT}")
    #    list(APPEND DOCOPT_SOURCES_OUTPUT "${DOCOPT_OUTPUT_C}" PARENT_SCOPE)
    set(DOCOPT_SOURCES_OUTPUT
            "${DOCOPT_SOURCES_OUTPUT}"
            "${DOCOPT_OUTPUT_C}"
            PARENT_SCOPE)
    # --- Create the custom command -------------------------------------
    message(VERBOSE "Generating command for CLI handler for ${cli_command}")
    add_custom_command(
            OUTPUT "${DOCOPT_OUTPUT_C}" "${DOCOPT_OUTPUT_H}"
            COMMAND "${Python3_EXECUTABLE}"
            "${SRC_DIR}/scripts/docopt/docopt_c.py"
            --name
            "${cli_command}"
            --output-name
            "${DOCOPT_OUTPUT_C}"
            "${DOCOPT_INPUT}"
            DEPENDS "${DOCOPT_INPUT}"
            "${SRC_DIR}/scripts/docopt/docopt_c.py"
            COMMENT "Generating CLI handler for ${cli_command} to ${DOCOPT_OUTPUT}"
    )
endfunction()

message(STATUS "Generating CLI handler commands")
define_cli_command(avar)
define_cli_command(download)
define_cli_command(queue)
define_cli_command(config)
define_cli_command(daemon)
define_cli_command(profile)
define_cli_command(scheduler)
define_cli_command(batch)

set(DOCOPT_INPUT1 ${SRC_DIR}/cli/avar.docopt)
set(DOCOPT_OUTPUT1 ${SOURCE_GEN_DIR}/cli_handler.c)
add_custom_command(
        OUTPUT ${DOCOPT_OUTPUT1}
        COMMAND ${Python3_EXECUTABLE}
        ${SRC_DIR}/scripts/docopt/docopt_c.py
        --output-name
        ${DOCOPT_OUTPUT1}
        ${DOCOPT_INPUT1}
        DEPENDS ${DOCOPT_INPUT1} ${SRC_DIR}/scripts/docopt/docopt_c.py
)

include_directories(
        src
        src/include
        src/third_party
        src/third_party/docopt
        src/third_party/mongoose
        ${SOURCE_GEN_DIR}
)

add_definitions(-DMG_ENABLE_CUSTOM_LOG=1)

add_executable(${PROJECT_NAME}
        "${SRC_DIR}/main.c"
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/download.c"
        "${SRC_DIR}/profile.c"
        "${SRC_DIR}/queue.c"
        "${SRC_DIR}/scheduler.c"
        "${SRC_DIR}/transport.c"
        "${SRC_DIR}/torrent.c"
        "${SRC_DIR}/daemon.c"
        "${SRC_DIR}/cli.c"
        "${SRC_DIR}/third_party/mongoose/mongoose.c"

        "${SRC_DIR}/cli.c"
        "${DOCOPT_SOURCES_OUTPUT}"
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} m pthread)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} m pthread)
endif()
