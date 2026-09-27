# Builds and links the Avar C daemon core into the Qt desktop GUI (optional).
if(AVAR_GUI_QT_WASM)
    return()
endif()

set(_avar_root "${CMAKE_CURRENT_LIST_DIR}/../..")
if(NOT EXISTS "${_avar_root}/CMakeLists.txt")
    message(FATAL_ERROR "Avar backend not found at ${_avar_root}")
endif()

set(AVAR_AS_GUI_QT_SUBPROJECT ON)
set(AVAR_REPO_ROOT "${_avar_root}")
set(AVAR_BUILD_EXECUTABLE OFF CACHE BOOL "Embedded in gui-qt" FORCE)
set(AVAR_BUILD_GUI OFF CACHE BOOL "" FORCE)
set(AVAR_BUILD_ALL OFF CACHE BOOL "" FORCE)
set(AVAR_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(AVAR_ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)

add_subdirectory("${_avar_root}" "${CMAKE_BINARY_DIR}/avar-backend" EXCLUDE_FROM_ALL)

target_link_libraries(avar-gui-qt PRIVATE avar_core avar_cli)
target_compile_definitions(avar-gui-qt PRIVATE AVAR_GUI_QT_EMBED_BACKEND=1)
target_include_directories(avar-gui-qt
    PRIVATE
        "${_avar_root}/src"
        "${_avar_root}/src/include"
        "${_avar_root}/src/third_party"
        "${_avar_root}/src/third_party/cJSON"
)
