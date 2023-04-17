CPMAddPackage(
        doctest
        GIT_REPOSITORY https://gitee.com/acking-you/doctest.git
        GIT_TAG v2.4.9
        GIT_SHALLOW TRUE
)

CPMAddPackage(
        nanobench
        GIT_REPOSITORY https://gitee.com/acking-you/nanobench.git
        GIT_TAG v4.1.0
        GIT_SHALLOW TRUE)

file(GLOB TEST_FILES
        "${PROJECT_SOURCE_DIR}/tests/*_test.cc"
        )


add_executable(unittest ${TEST_FILES})
target_link_libraries(unittest PRIVATE doctest_with_main nanobench netpoll)

add_test(NAME unittest COMMAND unittest)