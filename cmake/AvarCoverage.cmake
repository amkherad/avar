# Coverage instrumentation for GCC and Clang.

option(AVAR_ENABLE_COVERAGE "Enable gcov coverage instrumentation" OFF)

function(avar_apply_coverage target)
    if(NOT AVAR_ENABLE_COVERAGE)
        return()
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE --coverage -O0 -g)
        target_link_options(${target} PRIVATE --coverage)
    else()
        message(WARNING "AVAR_ENABLE_COVERAGE is only supported with GCC or Clang")
    endif()
endfunction()
