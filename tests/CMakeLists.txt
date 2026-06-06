add_executable(test_utils
        test_utils.c
        "${SRC_DIR}/utils.c"
)

target_include_directories(test_utils PRIVATE
        "${SRC_DIR}"
        "${SRC_DIR}/include"
        "${THIRD_PARTY_DIR}/criterion/include"
        "${CMAKE_BINARY_DIR}/criterion/src"
)

target_compile_definitions(test_utils PRIVATE VERSION_STR=\"0.0.1\")

set_target_properties(test_utils PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
)

target_link_libraries(test_utils PRIVATE criterion)

add_dependencies(test_utils criterion)

if (WIN32)
    target_link_libraries(test_utils PRIVATE ws2_32)

    add_custom_command(TARGET test_utils POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:criterion>"
            "$<TARGET_FILE_DIR:test_utils>"
            COMMENT "Copy libcriterion runtime next to test_utils"
    )
elseif (UNIX AND NOT APPLE)
    target_link_libraries(test_utils PRIVATE m pthread)
elseif (APPLE)
    target_link_libraries(test_utils PRIVATE m pthread)
endif ()

add_test(NAME test_utils COMMAND test_utils)
