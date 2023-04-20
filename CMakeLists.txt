cmake_minimum_required(VERSION 3.14)
project(netpoll)

# 检测C++版本，并给出提示
if (CMAKE_CXX_STANDARD LESS 14)
    message(FATAL_ERROR "CMAKE_CXX_STANDARD must be at least 14")
elseif (CMAKE_CXX_STANDARD EQUAL 14)
    message(STATUS "Using C++14")
else ()
    message(STATUS "Using C++17")
    set(CMAKE_CXX_STANDARD 17)
endif ()

#防止依赖源码的情况下产生重复的目标
if (TARGET ${PROJECT_NAME})
    message(WARNING "Target ${PROJECT_NAME} already exists!")
    return()
endif ()
#获取依赖
include(${PROJECT_SOURCE_DIR}/dependencies/fetch.cmake)


add_library(${PROJECT_NAME})

file(GLOB ${PROJECT_NAME}_util ${PROJECT_SOURCE_DIR}/netpoll/util/*.cc ${PROJECT_SOURCE_DIR}/netpoll/util/*.h)

file(GLOB ${PROJECT_NAME}_net
        ${PROJECT_SOURCE_DIR}/netpoll/net/*.cc
        ${PROJECT_SOURCE_DIR}/netpoll/net/*.h
        ${PROJECT_SOURCE_DIR}/netpoll/net/inner/*.h
        ${PROJECT_SOURCE_DIR}/netpoll/net/inner/*.cc
        ${PROJECT_SOURCE_DIR}/netpoll/net/inner/poller/*.h
        ${PROJECT_SOURCE_DIR}/netpoll/net/inner/poller/*.cc
        )
file(GLOB ${PROJECT_NAME}_wrap ${PROJECT_SOURCE_DIR}/netpoll/wrap/*.cc ${PROJECT_SOURCE_DIR}/netpoll/wrap/*.h)

if (WIN32)
    list(APPEND ${PROJECT_NAME}_net ${PROJECT_SOURCE_DIR}/dependencies/wepoll/wepoll.c ${PROJECT_SOURCE_DIR}/dependencies/windows_support.cc)
endif ()

target_sources(${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_util} ${${PROJECT_NAME}_net} ${${PROJECT_NAME}_wrap})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} elog)


option(BUILD_TEST "BUILD_TEST" OFF)
option(BUILD_EXAMPLE "BUILD_EXAMPLE" OFF)

if (BUILD_TEST)
    message(STATUS "Start BUILD_TEST")
    enable_testing()  # Enable testing only works in root scope
    add_subdirectory(tests)
endif ()

if (BUILD_EXAMPLE)
    message(STATUS "Start BUILD_EXAMPLE")
    add_subdirectory(examples)
endif ()
