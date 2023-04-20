add_subdirectory(http)

file(GLOB SINGLE_FILES
        "${PROJECT_SOURCE_DIR}/examples/*_example.cc"
        )
foreach (file ${SINGLE_FILES})
    get_filename_component(target ${file} NAME_WLE)
    add_executable(${target} ${file})
    target_link_libraries(${target} PRIVATE netpoll)
endforeach ()