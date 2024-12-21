include(${PROJECT_SOURCE_DIR}/cmake/add_executable_test.cmake)
CPMAddPackage(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
        GIT_SHALLOW TRUE
)

CPMAddPackage(
        nanobench
        GIT_REPOSITORY https://github.com/martinus/nanobench.git
        GIT_TAG v4.3.11
        GIT_SHALLOW TRUE)

file(GLOB TEST_FILES
        "${PROJECT_SOURCE_DIR}/tests/*_test.cc"
)

if ((CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif ()

foreach (test_file ${TEST_FILES})
    add_executable_test_by_filename(${test_file})
    target_link_libraries(${result} PRIVATE doctest_with_main nanobench netpoll)
endforeach ()