option(AVAR_GUI_QT_WASM "Cross-compile for Qt WebAssembly (Emscripten)" OFF)
option(AVAR_GUI_QT_BUILD_TESTS "Build gui-qt unit tests" ON)
option(AVAR_GUI_QT_ENABLE_EXTENSION_SUBPROCESS
    "Launch extensions/daemon bridge as a child process on desktop"
    ON)

set(AVAR_GUI_QT_CXX_STANDARD 23)

function(avar_gui_qt_apply_target_options target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD ${AVAR_GUI_QT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        WIN32_EXECUTABLE $<NOT:$<BOOL:${AVAR_GUI_QT_WASM}>>
    )

    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        QT_STRICT_ITERATORS
        QT_USE_QSTRINGBUILDER
    )

    if(AVAR_GUI_QT_WASM)
        target_compile_definitions(${target} PRIVATE AVAR_GUI_HOSTING_WASM=1)
    else()
        target_compile_definitions(${target} PRIVATE AVAR_GUI_HOSTING_DESKTOP=1)
    endif()

    if(AVAR_GUI_QT_ENABLE_EXTENSION_SUBPROCESS)
        target_compile_definitions(${target} PRIVATE AVAR_GUI_QT_ENABLE_EXTENSION_SUBPROCESS=1)
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(AVAR_GUI_QT_WASM)
    if(NOT CMAKE_TOOLCHAIN_FILE)
        message(FATAL_ERROR
            "AVAR_GUI_QT_WASM requires -DCMAKE_TOOLCHAIN_FILE=<Qt>/lib/cmake/Qt6/qt.toolchain.cmake")
    endif()
endif()
