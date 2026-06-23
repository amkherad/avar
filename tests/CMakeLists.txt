enable_testing()

set(AVAR_TEST_GUARD_SOURCES test_guard.c)

# Per-test CTest timeouts (seconds). Every test must use one of these tiers.
# Limits apply to the whole test executable (all subtests), not each case.
#
# fast        - pure logic, no blocking I/O
# unit        - config/filesystem/logger and other quick local I/O
# concurrent  - thread pool or pthread joins with intentional delays
# daemon      - subprocess/thread daemon bring-up and RPC
# integration - local HTTP server plus real downloads
# lifecycle   - download engine lifecycle with long idle waits
set(AVAR_TEST_TIMEOUT_FAST 60)
set(AVAR_TEST_TIMEOUT_UNIT 120)
set(AVAR_TEST_TIMEOUT_CONCURRENT 180)
set(AVAR_TEST_TIMEOUT_DAEMON 240)
set(AVAR_TEST_TIMEOUT_INTEGRATION 300)
set(AVAR_TEST_TIMEOUT_LIFECYCLE 360)

function(avar_add_test test_name)
    set(options)
    set(oneValueArgs TIMEOUT)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 1 _TEST "${options}" "${oneValueArgs}" "${multiValueArgs}")

    if(NOT _TEST_TIMEOUT)
        message(FATAL_ERROR "avar_add_test(${test_name}): TIMEOUT is required")
    endif()

    add_executable(${test_name} test_support.c ${_TEST_UNPARSED_ARGUMENTS})
    target_include_directories(${test_name} PRIVATE "${CMAKE_SOURCE_DIR}/tests")
    set_target_properties(${test_name} PROPERTIES
            C_STANDARD 23
            C_STANDARD_REQUIRED ON
    )
    avar_apply_coverage(${test_name})
    avar_apply_version(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT ${_TEST_TIMEOUT})
endfunction()

function(avar_link_core_test test_name)
    if(AVAR_ENABLE_COVERAGE)
        # daemon_rpc.c references CLI handlers; link the full backend when using objects.
        target_sources(${test_name} PRIVATE
                $<TARGET_OBJECTS:avar_core_obj>
                $<TARGET_OBJECTS:avar_cli_obj>
        )
        target_link_libraries(${test_name} PRIVATE avar_cli_testing)
    else()
        target_link_libraries(${test_name} PRIVATE avar_core_testing)
    endif()
endfunction()

function(avar_link_cli_test test_name)
    if(AVAR_ENABLE_COVERAGE)
        target_sources(${test_name} PRIVATE
                $<TARGET_OBJECTS:avar_core_obj>
                $<TARGET_OBJECTS:avar_cli_obj>
        )
        target_link_libraries(${test_name} PRIVATE avar_cli_testing)
    else()
        avar_link_cli_core(${test_name} avar_cli_testing avar_core_testing)
    endif()
endfunction()

avar_add_test(test_utils TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_utils.c)
avar_link_core_test(test_utils)

avar_add_test(test_http TIMEOUT ${AVAR_TEST_TIMEOUT_FAST} test_http.c)
avar_link_core_test(test_http)

avar_add_test(test_download_state TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_download_state.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_state)

avar_add_test(test_config_array TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_config_array.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_config_array)

avar_add_test(test_config_env TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_config_env.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_config_env)

avar_add_test(test_config_persist TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_config_persist.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_config_persist)

avar_add_test(test_queue TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_queue.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_queue)

avar_add_test(test_download_segment TIMEOUT ${AVAR_TEST_TIMEOUT_FAST} test_download_segment.c)
avar_link_core_test(test_download_segment)

avar_add_test(test_thread_pool TIMEOUT ${AVAR_TEST_TIMEOUT_CONCURRENT} test_thread_pool.c)
avar_link_core_test(test_thread_pool)

avar_add_test(test_instance TIMEOUT ${AVAR_TEST_TIMEOUT_FAST} test_instance.c)
avar_link_core_test(test_instance)

avar_add_test(test_http_proxy TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_http_proxy.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_http_proxy)

avar_add_test(test_file_system TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_file_system.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_file_system)

avar_add_test(test_logger TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_logger.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_logger)

avar_add_test(test_download_config TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_download_config.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_config)

avar_add_test(test_download_sync TIMEOUT ${AVAR_TEST_TIMEOUT_CONCURRENT} test_download_sync.c)
avar_link_core_test(test_download_sync)

avar_add_test(test_system_stats TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_system_stats.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_system_stats)

avar_add_test(test_stream_hls TIMEOUT ${AVAR_TEST_TIMEOUT_FAST} test_stream_hls.c)
avar_link_core_test(test_stream_hls)

avar_add_test(test_download_integration TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION}
        test_download_integration.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_integration)
target_compile_definitions(test_download_integration PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

avar_add_test(test_download_lifecycle TIMEOUT ${AVAR_TEST_TIMEOUT_LIFECYCLE}
        test_download_lifecycle.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_lifecycle)
target_compile_definitions(test_download_lifecycle PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

avar_add_test(test_download_internals TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_download_internals.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_internals)

avar_add_test(test_cli TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_cli.c)
avar_link_cli_test(test_cli)

avar_add_test(test_cli_handlers TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION} test_cli_handlers.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_cli_handlers)
target_compile_definitions(test_cli_handlers PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

avar_add_test(test_daemon TIMEOUT ${AVAR_TEST_TIMEOUT_DAEMON} test_daemon.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon)

avar_add_test(test_daemon_session TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_daemon_session.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon_session)

avar_add_test(test_daemon_rpc TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_daemon_rpc.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon_rpc)

avar_add_test(test_stream_hls_integration TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION}
        test_stream_hls_integration.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_stream_hls_integration)
target_compile_definitions(test_stream_hls_integration PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

avar_add_test(test_daemon_config TIMEOUT ${AVAR_TEST_TIMEOUT_UNIT} test_daemon_config.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon_config)

avar_add_test(test_daemon_transport TIMEOUT ${AVAR_TEST_TIMEOUT_DAEMON} test_daemon_transport.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon_transport)
