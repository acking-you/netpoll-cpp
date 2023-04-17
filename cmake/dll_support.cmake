function(dll_support)
    if (WIN32)
        foreach (target ${ARGN})
            message(STATUS "add callback to move dll:${target}")
            add_custom_command(
                    TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${target}>
                    COMMENT "Copying DLL files to ${target} directory"
            )
        endforeach ()
    else ()
        message(STATUS "os is not windows,so dll_support not need")
    endif ()
endfunction()