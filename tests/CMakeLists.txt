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
        "${SRC_DIR}/download_state.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_config_array
        test_config_array.c
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_queue
        test_queue.c
        "${SRC_DIR}/queue.c"
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

set(AVAR_DOWNLOAD_TEST_SOURCES
        "${SRC_DIR}/logger.c"
        "${SRC_DIR}/utils.c"
        "${SRC_DIR}/file-system.c"
        "${SRC_DIR}/config.c"
        "${SRC_DIR}/queue.c"
        "${SRC_DIR}/http.c"
        "${SRC_DIR}/download_state.c"
        "${SRC_DIR}/download.c"
        "${SRC_DIR}/third_party/mongoose/mongoose.c"
        "${SRC_DIR}/third_party/cJSON/cJSON.c"
)

avar_add_test(test_download_integration
        test_download_integration.c
        ${AVAR_DOWNLOAD_TEST_SOURCES}
)
target_compile_definitions(test_download_integration PRIVATE AVAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
