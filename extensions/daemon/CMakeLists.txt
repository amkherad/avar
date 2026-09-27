cmake_minimum_required(VERSION 3.16)

project(avar-extension-daemon
    VERSION 0.1.0
    DESCRIPTION "Standalone HTTP bridge between browser extensions and the Avar GUI/daemon"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(avar-extension-daemon
    src/main.c
    src/extension_protocol.c
    src/http_server.c
    src/bridge_handlers.c
)

target_include_directories(avar-extension-daemon
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(avar-extension-daemon PRIVATE /W4)
else()
    target_compile_options(avar-extension-daemon PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(avar-extension-daemon PRIVATE pthread)
endif()

install(TARGETS avar-extension-daemon RUNTIME DESTINATION bin)
