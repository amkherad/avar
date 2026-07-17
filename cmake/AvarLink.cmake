# Link avar_cli and avar_core static libraries, resolving circular references.

function(avar_link_cli_core target cli_lib core_lib)
    if(APPLE)
        # ld64 does not support LINK_GROUP:RESCAN for C executables.
        target_link_libraries(${target} PRIVATE ${cli_lib} ${core_lib} ${cli_lib})
    else()
        target_link_libraries(${target} PRIVATE "$<LINK_GROUP:RESCAN,${cli_lib},${core_lib}>")
    endif()
endfunction()
