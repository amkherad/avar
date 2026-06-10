cmake_minimum_required(VERSION 4.1)
project(avar C CXX)

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
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/queue.c"
        "${SRC_DIR}/http.c"
        "${SRC_DIR}/download_state.c"
        "${SRC_DIR}/download.c"
        "${SRC_DIR}/transport.c"
        "${SRC_DIR}/torrent.c"
        "${SRC_DIR}/daemon/daemon.c"
        "${SRC_DIR}/daemon/daemon_config.c"
        "${SRC_DIR}/daemon/daemon_session.c"
        "${SRC_DIR}/daemon/daemon_transport.c"
        "${SRC_DIR}/daemon/daemon_rpc.c"
        "${SRC_DIR}/daemon/daemon_install.c"
        "${SRC_DIR}/third_party/mongoose/mongoose.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

add_executable(${PROJECT_NAME}
        "${SRC_DIR}/main.c"
        ${AVAR_CORE_SOURCES}
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

target_include_directories(${PROJECT_NAME} PRIVATE
        src
        src/include
        src/third_party
        src/third_party/argtable3
        src/third_party/mongoose
        src/third_party/cJSON
        "${THIRD_PARTY_DIR}/mbedtls/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        MG_ENABLE_CUSTOM_LOG=1
        MG_TLS=MG_TLS_MBED
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mbedtls_vendor)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread mbedtls_vendor)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread mbedtls_vendor)
endif()

add_subdirectory(tests)

set(DEBUG 1)
