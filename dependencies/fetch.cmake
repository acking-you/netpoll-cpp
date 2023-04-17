include(${PROJECT_SOURCE_DIR}/cmake/CPM.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/target_if_include.cmake)

set(fetch_root_path ${PROJECT_SOURCE_DIR}/dependencies)

target_if_include(elog ${fetch_root_path}/elog4cpp/fetch_elog4cpp.cmake)