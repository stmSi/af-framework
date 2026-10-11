file(GLOB_RECURSE sources
    "${AF_FRAMEWORK_ROOT}/include/*.h"
    "${AF_FRAMEWORK_ROOT}/src/*.c"
    "${AF_FRAMEWORK_ROOT}/src/*.inc")
foreach(source IN LISTS sources)
    file(STRINGS "${source}" includes REGEX "#include[ \t]+\"")
    get_filename_component(directory "${source}" DIRECTORY)
    foreach(include IN LISTS includes)
        string(REGEX REPLACE ".*#include[ \t]+\"([^\"]+)\".*" "\\1" name "${include}")
        if(NOT EXISTS "${AF_FRAMEWORK_ROOT}/include/${name}" AND
           NOT EXISTS "${AF_FRAMEWORK_ROOT}/src/${name}" AND
           NOT EXISTS "${directory}/${name}")
            message(FATAL_ERROR "Framework include is not self-contained: ${source}: ${name}")
        endif()
    endforeach()
endforeach()
message(STATUS "Framework source dependencies stay inside the framework")
