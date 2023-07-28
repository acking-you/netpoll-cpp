macro(add_executable_test target path)
    add_executable(${target} ${path} ${ARGN})
    add_test(NAME ${target} COMMAND ${target})
endmacro()

# only useful in same dir level
function(add_executable_test_by_filename filename)
    get_filename_component(target ${filename} NAME_WLE)
    set(result ${target} PARENT_SCOPE)
    add_executable_test(${target} ${filename} ${ARGN})
endfunction()
