enable_testing()

set(AVAR_TEST_GUARD_SOURCES test_guard.c)
set(AVAR_INSTANCE_SOURCE "${SRC_DIR}/instance.c")

function(avar_add_test test_name)
    add_executable(${test_name} ${ARGN})
    target_include_directories(${test_name} PRIVATE
            "${CMAKE_SOURCE_DIR}/tests"
            "${SRC_DIR}"
            "${SRC_DIR}/include"
            "${THIRD_PARTY_DIR}"
            "${THIRD_PARTY_DIR}/argtable3"
            "${THIRD_PARTY_DIR}/mongoose"
            "${THIRD_PARTY_DIR}/cJSON"
            "${THIRD_PARTY_DIR}/mbedtls/include"
    )
    target_compile_definitions(${test_name} PRIVATE
            MG_TLS=MG_TLS_MBED
            AVAR_TESTING=1
    )
    set_target_properties(${test_name} PROPERTIES
            C_STANDARD 23
            C_STANDARD_REQUIRED ON
    )
    if (WIN32)
        target_link_libraries(${test_name} PRIVATE ws2_32 mbedtls_vendor)
    else()
        target_link_libraries(${test_name} PRIVATE m pthread mbedtls_vendor)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

avar_add_test(test_utils
        test_utils.c
        "${SRC_DIR}/utils.c"
)
target_compile_definitions(test_utils PRIVATE VERSION_STR=\"0.0.1\")

avar_add_test(test_http
        test_http.c
        "${SRC_DIR}/http.c"
        "${SRC_DIR}/third_party/mongoose/mongoose.c"
)

avar_add_test(test_download_state
        test_download_state.c
        ${AVAR_TEST_GUARD_SOURCES}
        "${SRC_DIR}/download_state.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_config_array
        test_config_array.c
        ${AVAR_TEST_GUARD_SOURCES}
        ${AVAR_INSTANCE_SOURCE}
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_config_env
        test_config_env.c
        ${AVAR_TEST_GUARD_SOURCES}
        ${AVAR_INSTANCE_SOURCE}
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_queue
        test_queue.c
        ${AVAR_TEST_GUARD_SOURCES}
        ${AVAR_INSTANCE_SOURCE}
        "${SRC_DIR}/queue.c"
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_download_segment
        test_download_segment.c
        "${SRC_DIR}/download_state.c"
        "${SRC_DIR}/download_segment.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_thread_pool
        test_thread_pool.c
        "${SRC_DIR}/thread_pool.c"
)

avar_add_test(test_instance
        test_instance.c
        ${AVAR_INSTANCE_SOURCE}
)

set(AVAR_DOWNLOAD_TEST_SOURCES
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        ${AVAR_INSTANCE_SOURCE}
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
        "${SRC_DIR}/system_stats.c"
        "${SRC_DIR}/third_party/mongoose/mongoose.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_download_integration
        test_download_integration.c
        ${AVAR_TEST_GUARD_SOURCES}
        ${AVAR_DOWNLOAD_TEST_SOURCES}
)
target_compile_definitions(test_download_integration PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

set(AVAR_DAEMON_TEST_SOURCES
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        ${AVAR_INSTANCE_SOURCE}
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
        "${SRC_DIR}/system_stats.c"
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

avar_add_test(test_daemon
        test_daemon.c
        ${AVAR_TEST_GUARD_SOURCES}
        ${AVAR_DAEMON_TEST_SOURCES}
)
target_compile_definitions(test_daemon PRIVATE VERSION_STR=\"0.0.1\")
