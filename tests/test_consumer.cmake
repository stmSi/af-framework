if(NOT AF_FRAMEWORK_BINARY_ROOT OR NOT AF_FRAMEWORK_CONSUMER_ROOT OR
   NOT EXISTS "${AF_FRAMEWORK_BINARY_ROOT}/cmake_install.cmake")
    message(FATAL_ERROR "A configured framework build and consumer source directory are required")
endif()
set(prefix "${AF_FRAMEWORK_BINARY_ROOT}/consumer-prefix")
set(build "${AF_FRAMEWORK_BINARY_ROOT}/consumer-build")
file(REMOVE_RECURSE "${prefix}" "${build}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${AF_FRAMEWORK_BINARY_ROOT}" --prefix "${prefix}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Framework SDK installation failed: ${output}\n${error}")
endif()
set(toolchain_arguments)
if(AF_FRAMEWORK_TOOLCHAIN_FILE)
    list(APPEND toolchain_arguments "-DCMAKE_TOOLCHAIN_FILE=${AF_FRAMEWORK_TOOLCHAIN_FILE}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${AF_FRAMEWORK_CONSUMER_ROOT}" -B "${build}"
    ${toolchain_arguments}
    "-DCMAKE_PREFIX_PATH=${prefix}"
    "-DAF_CONSUMER_FREETYPE_LIBRARY=${AF_CONSUMER_FREETYPE_LIBRARY}"
    "-DAF_CONSUMER_HARFBUZZ_LIBRARY=${AF_CONSUMER_HARFBUZZ_LIBRARY}"
    "-DSDL3_DIR=${AF_CONSUMER_SDL_DIR}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Independent SDK consumer configuration failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${build}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Independent SDK consumer build failed: ${output}\n${error}")
endif()
if(NOT AF_FRAMEWORK_CROSSCOMPILING)
    execute_process(COMMAND "${build}/af_framework_consumer${AF_FRAMEWORK_EXECUTABLE_SUFFIX}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Independent SDK consumer failed: ${output}\n${error}")
    endif()
endif()
message(STATUS "Installed framework builds and runs an independent application")
