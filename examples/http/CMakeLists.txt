aux_source_directory(./ HTTP_SRC)
add_executable(http_example ${HTTP_SRC})

add_definitions(-DBASE_FILES_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(http_example ${PROJECT_NAME})