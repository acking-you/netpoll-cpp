aux_source_directory(./ HTTP_SRC)
add_executable(http_example ${HTTP_SRC})

target_link_libraries(http_example ${PROJECT_NAME})