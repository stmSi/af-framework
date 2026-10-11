cmake_minimum_required(VERSION 3.25)
project(AFFramework VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(AF_FRAMEWORK_BUILD_PLATFORM "Build SDL window and input integration" ON)
option(AF_FRAMEWORK_BUILD_VULKAN "Build the Vulkan renderer" ON)
option(AF_FRAMEWORK_ENABLE_VALIDATION "Enable Vulkan validation when available" ON)
option(AF_FRAMEWORK_BUILD_TESTS "Build framework tests" ${PROJECT_IS_TOP_LEVEL})
option(AF_FRAMEWORK_BUILD_EXAMPLES "Build the standalone framework example" ${PROJECT_IS_TOP_LEVEL})
set(AF_FRAMEWORK_DEPENDENCY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH
    "Optional directory containing pinned SDL, FreeType, HarfBuzz, Vulkan-Headers and volk")
set(AF_FRAMEWORK_DEPENDENCY_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party" CACHE PATH
    "Build directory for bundled dependencies")
set(AF_FRAMEWORK_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders" CACHE PATH
    "Output directory for compiled framework shaders")

if(AF_FRAMEWORK_BUILD_VULKAN AND NOT AF_FRAMEWORK_BUILD_PLATFORM)
    message(FATAL_ERROR "The Vulkan window renderer requires AF_FRAMEWORK_BUILD_PLATFORM")
endif()
include(cmake/Dependencies.cmake)

function(af_framework_library target name)
    add_library(${target} STATIC ${ARGN})
    add_library(af::${name} ALIAS ${target})
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME ${name})
    target_include_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/af-framework>"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wshadow -Wstrict-prototypes)
    endif()
    install(TARGETS ${target} EXPORT AFFrameworkTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endfunction()

af_framework_library(af_foundation foundation
    src/core/allocator.c src/core/array.c src/core/arena.c src/core/error.c
    src/core/file_io.c src/core/handles.c src/core/hash.c src/core/log.c
    src/core/math.c src/core/png.c)
if(UNIX)
    target_link_libraries(af_foundation PUBLIC m)
endif()

af_framework_library(af_engine engine
    src/scene/scene.c src/animation/animation.c src/animation/advanced_animation.c
    src/path/path.c src/path/path_boolean.c src/path/path_offset.c src/style/style.c
    src/layout/layout.c src/text/text.c src/binding/binding.c src/localization/localization.c
    src/performance/metrics.c src/stability/diagnostics.c src/component/component.c
    src/interaction/interaction.c src/state_machine/state_machine.c src/composition/composition.c
    src/deformation/deformation.c src/rigging/rigging.c src/particle/particle.c src/assets/image.c)
target_link_libraries(af_engine PUBLIC af_foundation PRIVATE
    "$<BUILD_INTERFACE:freetype>" "$<INSTALL_INTERFACE:AFFramework::Freetype>"
    "$<BUILD_INTERFACE:harfbuzz>" "$<INSTALL_INTERFACE:AFFramework::HarfBuzz>")

af_framework_library(af_ui ui src/ui/draw.c src/ui/input.c src/ui/actions.c)
target_link_libraries(af_ui PUBLIC af_engine)

if(AF_FRAMEWORK_BUILD_PLATFORM)
    af_framework_library(af_platform platform src/platform/platform_sdl.c)
    target_link_libraries(af_platform PUBLIC af_foundation
        "$<BUILD_INTERFACE:SDL3::SDL3-static>" "$<INSTALL_INTERFACE:AFFramework::SDL3>")
endif()

if(AF_FRAMEWORK_BUILD_VULKAN)
    include(cmake/Shaders.cmake)
    af_framework_library(af_volk volk "${AF_FRAMEWORK_VOLK_SOURCE}")
    target_include_directories(af_volk PRIVATE "${AF_FRAMEWORK_VOLK_INCLUDE_DIR}"
        "${AF_FRAMEWORK_VULKAN_INCLUDE_DIR}")
    if(UNIX AND NOT APPLE)
        target_link_libraries(af_volk PUBLIC dl)
    endif()
    af_framework_library(af_vulkan vulkan src/renderer/vk_particles.c src/renderer/vk_renderer.c)
    target_include_directories(af_vulkan PRIVATE "${AF_FRAMEWORK_VOLK_INCLUDE_DIR}"
        "${AF_FRAMEWORK_VULKAN_INCLUDE_DIR}")
    target_include_directories(af_vulkan PUBLIC
        "$<BUILD_INTERFACE:${AF_FRAMEWORK_VULKAN_INCLUDE_DIR}>")
    target_compile_definitions(af_vulkan PRIVATE VK_NO_PROTOTYPES
        AF_FRAMEWORK_SHADER_DIR="${AF_FRAMEWORK_SHADER_OUTPUT_DIR}"
        $<$<BOOL:${AF_FRAMEWORK_ENABLE_VALIDATION}>:AF_ENABLE_VALIDATION=1>)
    target_link_libraries(af_vulkan PUBLIC af_engine af_platform PRIVATE af_volk)
    add_dependencies(af_vulkan af_framework_shaders)
    install(DIRECTORY "${AF_FRAMEWORK_VULKAN_INCLUDE_DIR}/vulkan"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/af-framework")
    if(EXISTS "${AF_FRAMEWORK_VULKAN_INCLUDE_DIR}/vk_video")
        install(DIRECTORY "${AF_FRAMEWORK_VULKAN_INCLUDE_DIR}/vk_video"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/af-framework")
    endif()
    install(FILES ${AF_FRAMEWORK_SHADER_FILES}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/af-framework/shaders")
endif()

add_library(af_framework INTERFACE)
add_library(af::framework ALIAS af_framework)
set_target_properties(af_framework PROPERTIES EXPORT_NAME framework)
target_link_libraries(af_framework INTERFACE af_ui)
if(AF_FRAMEWORK_BUILD_PLATFORM)
    target_link_libraries(af_framework INTERFACE af_platform)
endif()
if(AF_FRAMEWORK_BUILD_VULKAN)
    target_link_libraries(af_framework INTERFACE af_vulkan)
endif()
install(TARGETS af_framework EXPORT AFFrameworkTargets)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/af-framework")
install(EXPORT AFFrameworkTargets NAMESPACE af::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AFFramework")
configure_package_config_file(cmake/AFFrameworkConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/AFFrameworkConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AFFramework")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/AFFrameworkConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AFFrameworkConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AFFrameworkConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AFFramework")

if(AF_FRAMEWORK_BUILD_TESTS)
    enable_testing()
    set(AF_FRAMEWORK_TEST_FREETYPE_LIBRARY "")
    set(AF_FRAMEWORK_TEST_HARFBUZZ_LIBRARY "")
    get_target_property(freetype_type freetype TYPE)
    get_target_property(harfbuzz_type harfbuzz TYPE)
    if(NOT freetype_type STREQUAL "INTERFACE_LIBRARY")
        set(AF_FRAMEWORK_TEST_FREETYPE_LIBRARY "$<TARGET_FILE:freetype>")
    endif()
    if(NOT harfbuzz_type STREQUAL "INTERFACE_LIBRARY")
        set(AF_FRAMEWORK_TEST_HARFBUZZ_LIBRARY "$<TARGET_FILE:harfbuzz>")
    endif()
    add_executable(af_framework_tests tests/test_framework.c)
    target_link_libraries(af_framework_tests PRIVATE af_ui)
    add_test(NAME af_framework_tests COMMAND af_framework_tests)
    add_test(NAME af_framework_boundary_tests
        COMMAND "${CMAKE_COMMAND}" "-DAF_FRAMEWORK_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_boundaries.cmake")
    add_test(NAME af_framework_consumer_tests
        COMMAND "${CMAKE_COMMAND}"
            "-DAF_FRAMEWORK_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
            "-DAF_FRAMEWORK_CONSUMER_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/tests/consumer"
            "-DAF_CONSUMER_FREETYPE_LIBRARY=${AF_FRAMEWORK_TEST_FREETYPE_LIBRARY}"
            "-DAF_CONSUMER_HARFBUZZ_LIBRARY=${AF_FRAMEWORK_TEST_HARFBUZZ_LIBRARY}"
            "-DAF_CONSUMER_SDL_DIR=${AF_FRAMEWORK_DEPENDENCY_BINARY_DIR}/SDL"
            "-DAF_FRAMEWORK_CROSSCOMPILING=${CMAKE_CROSSCOMPILING}"
            "-DAF_FRAMEWORK_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
            "-DAF_FRAMEWORK_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_consumer.cmake")
endif()
if(AF_FRAMEWORK_BUILD_EXAMPLES AND AF_FRAMEWORK_BUILD_VULKAN)
    add_executable(af_framework_demo examples/demo.c)
    target_link_libraries(af_framework_demo PRIVATE af::framework)
endif()
