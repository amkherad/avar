# Link avar_cli and avar_core static libraries, resolving circular references.

function(avar_link_cli_core target cli_lib core_lib)
    target_link_libraries(${target} PRIVATE "$<LINK_GROUP:RESCAN,${cli_lib},${core_lib}>")
endfunction()
