enable_testing()

set(AVAR_TEST_GUARD_SOURCES test_guard.c)

# CTest timeouts (seconds). Integration tests spawn HTTP servers and may wait on downloads.
set(AVAR_TEST_TIMEOUT_DEFAULT 30)
set(AVAR_TEST_TIMEOUT_INTEGRATION 180)
set(AVAR_TEST_TIMEOUT_LIFECYCLE 240)

function(avar_add_test test_name)
    set(options)
    set(oneValueArgs TIMEOUT)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 1 _TEST "${options}" "${oneValueArgs}" "${multiValueArgs}")

    if(NOT _TEST_TIMEOUT)
        set(_TEST_TIMEOUT ${AVAR_TEST_TIMEOUT_DEFAULT})
    endif()

    add_executable(${test_name} test_support.c ${_TEST_UNPARSED_ARGUMENTS})
    target_include_directories(${test_name} PRIVATE "${CMAKE_SOURCE_DIR}/tests")
    set_target_properties(${test_name} PROPERTIES
            C_STANDARD 23
            C_STANDARD_REQUIRED ON
    )
    avar_apply_coverage(${test_name})
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
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_libraries(${test_name} PRIVATE -Wl,--start-group avar_cli_testing avar_core_testing -Wl,--end-group)
    else()
        target_link_libraries(${test_name} PRIVATE avar_cli_testing avar_core_testing)
    endif()
endfunction()

avar_add_test(test_utils test_utils.c)
avar_link_core_test(test_utils)
target_compile_definitions(test_utils PRIVATE VERSION_STR=\"0.0.1\")

avar_add_test(test_http test_http.c)
avar_link_core_test(test_http)

avar_add_test(test_download_state test_download_state.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_state)

avar_add_test(test_config_array test_config_array.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_config_array)

avar_add_test(test_config_env test_config_env.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_config_env)

avar_add_test(test_config_persist test_config_persist.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_config_persist)

avar_add_test(test_queue test_queue.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_queue)

avar_add_test(test_download_segment test_download_segment.c)
avar_link_core_test(test_download_segment)

avar_add_test(test_thread_pool test_thread_pool.c)
avar_link_core_test(test_thread_pool)

avar_add_test(test_instance test_instance.c)
avar_link_core_test(test_instance)

avar_add_test(test_http_proxy TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION} test_http_proxy.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_http_proxy)

avar_add_test(test_file_system test_file_system.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_file_system)

avar_add_test(test_logger test_logger.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_logger)

avar_add_test(test_download_config test_download_config.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_config)

avar_add_test(test_download_sync test_download_sync.c)
avar_link_core_test(test_download_sync)

avar_add_test(test_system_stats test_system_stats.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_system_stats)

avar_add_test(test_stream_hls test_stream_hls.c)
avar_link_core_test(test_stream_hls)

avar_add_test(test_download_integration TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION}
        test_download_integration.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_integration)
target_compile_definitions(test_download_integration PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

avar_add_test(test_download_lifecycle TIMEOUT ${AVAR_TEST_TIMEOUT_LIFECYCLE}
        test_download_lifecycle.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_lifecycle)
target_compile_definitions(test_download_lifecycle PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

avar_add_test(test_download_internals test_download_internals.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_download_internals)

avar_add_test(test_cli test_cli.c)
avar_link_cli_test(test_cli)
target_compile_definitions(test_cli PRIVATE VERSION_STR=\"0.0.1\")

avar_add_test(test_cli_handlers TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION} test_cli_handlers.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_cli_handlers)
target_compile_definitions(test_cli_handlers PRIVATE VERSION_STR=\"0.0.1\" AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

avar_add_test(test_daemon TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION} test_daemon.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon)
target_compile_definitions(test_daemon PRIVATE VERSION_STR=\"0.0.1\")

avar_add_test(test_daemon_session test_daemon_session.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon_session)

avar_add_test(test_daemon_rpc test_daemon_rpc.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon_rpc)

avar_add_test(test_stream_hls_integration TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION}
        test_stream_hls_integration.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_core_test(test_stream_hls_integration)
target_compile_definitions(test_stream_hls_integration PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

avar_add_test(test_daemon_config test_daemon_config.c ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon_config)

avar_add_test(test_daemon_transport TIMEOUT ${AVAR_TEST_TIMEOUT_INTEGRATION} test_daemon_transport.c
        ${AVAR_TEST_GUARD_SOURCES})
avar_link_cli_test(test_daemon_transport)
target_compile_definitions(test_daemon_transport PRIVATE VERSION_STR=\"0.0.1\")
