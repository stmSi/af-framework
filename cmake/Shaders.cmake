set(AF_FRAMEWORK_SHADER_TOOLCHAIN "${AF_FRAMEWORK_DEPENDENCY_ROOT}/shader-toolchain/usr")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   EXISTS "${AF_FRAMEWORK_SHADER_TOOLCHAIN}/bin/glslc" AND
   EXISTS "${AF_FRAMEWORK_SHADER_TOOLCHAIN}/bin/spirv-val")
    set(AF_FRAMEWORK_GLSLC "${AF_FRAMEWORK_SHADER_TOOLCHAIN}/bin/glslc")
    set(AF_FRAMEWORK_SPIRV_VAL "${AF_FRAMEWORK_SHADER_TOOLCHAIN}/bin/spirv-val")
    set(AF_FRAMEWORK_SHADER_ENV "${CMAKE_COMMAND}" -E env
        "LD_LIBRARY_PATH=${AF_FRAMEWORK_SHADER_TOOLCHAIN}/lib")
else()
    find_program(AF_FRAMEWORK_GLSLC glslc REQUIRED)
    find_program(AF_FRAMEWORK_SPIRV_VAL spirv-val REQUIRED)
    set(AF_FRAMEWORK_SHADER_ENV)
endif()
set(AF_FRAMEWORK_SHADER_FILES)
foreach(shader analytic.vert analytic.frag path.vert path.frag image.vert image.frag
               particle.comp particle.vert particle.frag)
    set(output "${AF_FRAMEWORK_SHADER_OUTPUT_DIR}/${shader}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AF_FRAMEWORK_SHADER_OUTPUT_DIR}"
        COMMAND ${AF_FRAMEWORK_SHADER_ENV} "${AF_FRAMEWORK_GLSLC}" --target-env=vulkan1.3 -O
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${output}"
        COMMAND ${AF_FRAMEWORK_SHADER_ENV} "${AF_FRAMEWORK_SPIRV_VAL}" --target-env vulkan1.3 "${output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/paint.glsl" VERBATIM)
    list(APPEND AF_FRAMEWORK_SHADER_FILES "${output}")
endforeach()
add_custom_target(af_framework_shaders ALL DEPENDS ${AF_FRAMEWORK_SHADER_FILES})
