cmake_minimum_required(VERSION 4.1)
project(avar C)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(THIRD_PARTY_DIR "${SRC_DIR}/third_party")

add_executable(${PROJECT_NAME}
        "${SRC_DIR}/main.c"
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/download.c"
        "${SRC_DIR}/transport.c"
        "${SRC_DIR}/torrent.c"
        "${SRC_DIR}/daemon.c"
        "${SRC_DIR}/cli/cli.c"
        "${SRC_DIR}/cli/dispatch.c"
        "${SRC_DIR}/cli/download_cli.c"
        "${SRC_DIR}/cli/queue_cli.c"
        "${SRC_DIR}/cli/config_cli.c"
        "${SRC_DIR}/cli/profile_cli.c"
        "${SRC_DIR}/cli/scheduler_cli.c"
        "${SRC_DIR}/cli/daemon_cli.c"
        "${SRC_DIR}/third_party/mongoose/mongoose.c"
        "${SRC_DIR}/third_party/argtable3/argtable3.c"
)

target_include_directories(${PROJECT_NAME} PRIVATE
        src
        src/include
        src/third_party
        src/third_party/argtable3
        src/third_party/mongoose
)

target_compile_definitions(${PROJECT_NAME} PRIVATE MG_ENABLE_CUSTOM_LOG=1)

set_target_properties(${PROJECT_NAME} PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} m pthread)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} m pthread)
endif()
