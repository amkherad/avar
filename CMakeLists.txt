cmake_minimum_required(VERSION 4.1)
project(avar C CXX)

include("${CMAKE_SOURCE_DIR}/cmake/AvarCoverage.cmake")

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(THIRD_PARTY_DIR "${SRC_DIR}/third_party")

file(GLOB MBEDTLS_LIBRARY_SOURCES "${THIRD_PARTY_DIR}/mbedtls/library/*.c")
add_library(mbedtls_vendor STATIC ${MBEDTLS_LIBRARY_SOURCES})
target_include_directories(mbedtls_vendor PUBLIC "${THIRD_PARTY_DIR}/mbedtls/include")
if (MSVC)
    target_compile_options(mbedtls_vendor PRIVATE /wd4244 /wd4267)
endif()

set(AVAR_CORE_SOURCES
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/instance.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/queue.c"
        "${SRC_DIR}/http.c"
        "${SRC_DIR}/http_proxy.c"
        "${SRC_DIR}/download_state.c"
        "${SRC_DIR}/download_sync.c"
        "${SRC_DIR}/download_segment.c"
        "${SRC_DIR}/download_config.c"
        "${SRC_DIR}/thread_pool.c"
        "${SRC_DIR}/download.c"
        "${SRC_DIR}/stream_hls.c"
        "${SRC_DIR}/transport.c"
        "${SRC_DIR}/torrent.c"
        "${SRC_DIR}/system_stats.c"
        "${SRC_DIR}/daemon/daemon.c"
        "${SRC_DIR}/daemon/daemon_config.c"
        "${SRC_DIR}/daemon/daemon_session.c"
        "${SRC_DIR}/daemon/daemon_transport.c"
        "${SRC_DIR}/daemon/daemon_rpc.c"
        "${SRC_DIR}/daemon/daemon_install.c"
        "${SRC_DIR}/third_party/mongoose/mongoose.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
        "${SRC_DIR}/mongoose_log.c"
)

set(AVAR_CLI_SOURCES
        "${SRC_DIR}/cli/cli.c"
        "${SRC_DIR}/cli/dispatch.c"
        "${SRC_DIR}/cli/download_cli.c"
        "${SRC_DIR}/cli/queue_cli.c"
        "${SRC_DIR}/cli/config_cli.c"
        "${SRC_DIR}/cli/profile_cli.c"
        "${SRC_DIR}/cli/scheduler_cli.c"
        "${SRC_DIR}/daemon/daemon_cli.c"
        "${SRC_DIR}/third_party/argtable3/argtable3.c"
)

function(avar_configure_core_library target_name testing)
    target_include_directories(${target_name} PUBLIC
            src
            src/include
            src/third_party
            src/third_party/argtable3
            src/third_party/mongoose
            src/third_party/cJSON
            "${THIRD_PARTY_DIR}/mbedtls/include"
    )
    target_compile_definitions(${target_name} PUBLIC
            MG_ENABLE_CUSTOM_LOG=1
            MG_TLS=MG_TLS_MBED
    )
    if(testing)
        target_compile_definitions(${target_name} PUBLIC AVAR_TESTING=1)
    endif()
    set_target_properties(${target_name} PROPERTIES
            C_STANDARD 23
            C_STANDARD_REQUIRED ON
    )
    avar_apply_coverage(${target_name})
    if (WIN32)
        target_link_libraries(${target_name} PUBLIC ws2_32 mbedtls_vendor)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${target_name} PUBLIC m pthread mbedtls_vendor)
    elseif(APPLE)
        target_link_libraries(${target_name} PUBLIC m pthread mbedtls_vendor)
    endif()
endfunction()

function(avar_configure_cli_library target_name testing)
    target_include_directories(${target_name} PUBLIC
            src
            src/include
            src/third_party
            src/third_party/argtable3
            src/third_party/mongoose
            src/third_party/cJSON
            "${THIRD_PARTY_DIR}/mbedtls/include"
    )
    target_compile_definitions(${target_name} PUBLIC
            MG_ENABLE_CUSTOM_LOG=1
            MG_TLS=MG_TLS_MBED
    )
    if(testing)
        target_compile_definitions(${target_name} PUBLIC AVAR_TESTING=1)
    endif()
    set_target_properties(${target_name} PROPERTIES
            C_STANDARD 23
            C_STANDARD_REQUIRED ON
    )
    avar_apply_coverage(${target_name})
endfunction()

if(AVAR_ENABLE_COVERAGE)
    # Object libraries ensure gcov attributes hits to the same .gcda files when
    # many test executables link the backend (static archives duplicate profile
    # counters on some toolchains).
    add_library(avar_core_obj OBJECT ${AVAR_CORE_SOURCES})
    avar_configure_core_library(avar_core_obj TRUE)

    add_library(avar_cli_obj OBJECT ${AVAR_CLI_SOURCES})
    avar_configure_cli_library(avar_cli_obj TRUE)

    add_library(avar_core INTERFACE)
    target_include_directories(avar_core INTERFACE
            src
            src/include
            src/third_party
            src/third_party/argtable3
            src/third_party/mongoose
            src/third_party/cJSON
            "${THIRD_PARTY_DIR}/mbedtls/include"
    )
    target_compile_definitions(avar_core INTERFACE
            MG_ENABLE_CUSTOM_LOG=1
            MG_TLS=MG_TLS_MBED
            AVAR_TESTING=1
    )
    if(WIN32)
        target_link_libraries(avar_core INTERFACE ws2_32 mbedtls_vendor)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(avar_core INTERFACE m pthread mbedtls_vendor)
    elseif(APPLE)
        target_link_libraries(avar_core INTERFACE m pthread mbedtls_vendor)
    endif()

    add_library(avar_cli INTERFACE)
    target_link_libraries(avar_cli INTERFACE avar_core)
    if(WIN32)
        target_link_libraries(avar_cli INTERFACE ws2_32)
    endif()

    add_library(avar_core_testing ALIAS avar_core)
    add_library(avar_cli_testing ALIAS avar_cli)
else()
    add_library(avar_core_testing STATIC ${AVAR_CORE_SOURCES})
    avar_configure_core_library(avar_core_testing TRUE)

    add_library(avar_core STATIC ${AVAR_CORE_SOURCES})
    avar_configure_core_library(avar_core FALSE)

    add_library(avar_cli_testing STATIC ${AVAR_CLI_SOURCES})
    avar_configure_cli_library(avar_cli_testing TRUE)
    target_link_libraries(avar_cli_testing PUBLIC avar_core_testing)
    if(WIN32)
        target_link_libraries(avar_cli_testing PUBLIC ws2_32)
    endif()

    add_library(avar_cli STATIC ${AVAR_CLI_SOURCES})
    avar_configure_cli_library(avar_cli FALSE)
    target_link_libraries(avar_cli PUBLIC avar_core)
    if(WIN32)
        target_link_libraries(avar_cli PUBLIC ws2_32)
    endif()
endif()

add_executable(${PROJECT_NAME} "${SRC_DIR}/main.c")
if(AVAR_ENABLE_COVERAGE)
    target_sources(${PROJECT_NAME} PRIVATE
            $<TARGET_OBJECTS:avar_core_obj>
            $<TARGET_OBJECTS:avar_cli_obj>
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE avar_cli)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(${PROJECT_NAME} PRIVATE -Wl,--start-group avar_cli avar_core -Wl,--end-group)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE avar_cli)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
)
avar_apply_coverage(${PROJECT_NAME})

enable_testing()

add_subdirectory(tests)

set(DEBUG 1)

option(AVAR_BUILD_GUI "Build avar-gui executable with embedded web UI" OFF)

if(AVAR_BUILD_GUI)
    find_program(NPM_EXECUTABLE npm REQUIRED)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    set(GUI_DIST_DIR "${CMAKE_SOURCE_DIR}/gui/dist")
    set(GUI_EMBED_C "${CMAKE_BINARY_DIR}/generated/gui_embed.c")
    set(GUI_EMBED_H "${CMAKE_BINARY_DIR}/generated/gui_embed_generated.h")

  add_custom_command(
        OUTPUT "${GUI_EMBED_C}" "${GUI_EMBED_H}"
        COMMAND "${NPM_EXECUTABLE}" ci
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/gui"
        COMMAND "${NPM_EXECUTABLE}" run build
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/gui"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/embed_gui.py"
                --dist "${GUI_DIST_DIR}"
                --out-c "${GUI_EMBED_C}"
                --out-h "${GUI_EMBED_H}"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/gui/package.json"
            "${CMAKE_SOURCE_DIR}/scripts/embed_gui.py"
        COMMENT "Building and embedding GUI assets"
        VERBATIM
    )

    add_executable(avar-gui
            "${SRC_DIR}/main.c"
            "${GUI_EMBED_C}"
    )

    target_link_libraries(avar-gui PRIVATE avar_cli)
    target_include_directories(avar-gui PRIVATE
            "${CMAKE_BINARY_DIR}/generated"
    )

    target_compile_definitions(avar-gui PRIVATE AVAR_EMBED_GUI=1)

    set_target_properties(avar-gui PROPERTIES
            C_STANDARD 23
            C_STANDARD_REQUIRED ON
    )
    avar_apply_coverage(avar-gui)
endif()
