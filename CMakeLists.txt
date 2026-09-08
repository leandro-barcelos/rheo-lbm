cmake_minimum_required(VERSION 4.2)
project(rheo-lbm)

include(CTest)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(GDAL CONFIG REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(rheo_graphics_backend INTERFACE)
target_link_libraries(rheo_graphics_backend INTERFACE Vulkan::Vulkan)
target_compile_definitions(
    rheo_graphics_backend
    INTERFACE
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_FILE_DIALOG_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui-file-dialog
)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_FILE_DIALOG_DIR}/ImGuiFileDialog.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(
    imgui
    PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${Vulkan_INCLUDE_DIRS}
    ${IMGUI_FILE_DIALOG_DIR}
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)

target_compile_definitions(imgui PUBLIC VK_USE_PLATFORM_XLIB_KHR)

add_subdirectory(src/domain)
add_subdirectory(src/events)
add_subdirectory(src/platform)
add_subdirectory(src/graphics)
add_subdirectory(src/assets)
add_subdirectory(src/simulation)
add_subdirectory(src/application)
add_subdirectory(src/renderer)
add_subdirectory(src/ui)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_executable(${PROJECT_NAME} src/main.cpp src/rheo_lbm_app.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    rheo::application
    rheo::assets
    rheo::events
    rheo::graphics
    rheo::platform
    rheo::renderer
    rheo::simulation
    rheo::ui
)

find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

function(compile_slang_shaders TARGET_NAME SHADER_FILE OUTPUT_DIR OUTPUT_SPIRV)
    # Remaining args are forwarded directly to slangc (e.g. -entry foo -entry bar)
    set(ENTRY_ARGS ${ARGN})

    get_filename_component(
        SHADER_FILE_ABS
        "${SHADER_FILE}"
        ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}"
    )

    add_custom_command(
        OUTPUT ${OUTPUT_SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        COMMAND
        ${SLANGC_EXECUTABLE} ${SHADER_FILE_ABS} -target spirv -profile
        spirv_1_4+spvRayQueryKHR -emit-spirv-directly
        -fvk-use-entrypoint-name ${ENTRY_ARGS} -o ${OUTPUT_SPIRV}
        DEPENDS ${SHADER_FILE_ABS} "${CMAKE_SOURCE_DIR}/shaders/lattice_types.slang"
        "${CMAKE_SOURCE_DIR}/shaders/compute/common.slang"
        COMMENT "Compiling Slang Shaders: ${SHADER_FILE}"
        VERBATIM
    )
    add_custom_target(${TARGET_NAME} DEPENDS ${OUTPUT_SPIRV})
endfunction()

set(SHADERS_DIR shaders)

compile_slang_shaders(voxelize_terrain_shader "shaders/compute/voxelize_terrain.slang" "${SHADERS_DIR}/compute" "${SHADERS_DIR}/compute/voxelize_terrain.spv"
    -entry voxelize_terrain
)
compile_slang_shaders(lattice_render_shader "shaders/graphics/lattice.slang" "${SHADERS_DIR}/graphics" "${SHADERS_DIR}/graphics/lattice.spv"
    -entry vert_main -entry frag_main
)
add_dependencies(${PROJECT_NAME} voxelize_terrain_shader lattice_render_shader)

compile_slang_shaders(
    lattice_edit_shader
    "shaders/compute/lattice_editor.slang"
    "${SHADERS_DIR}/compute"
    "${SHADERS_DIR}/compute/lattice_editor.spv"
    -entry edit_cells
)
add_dependencies(rheo-lbm lattice_edit_shader)
compile_slang_shaders(
    collision_shader "shaders/compute/collision.slang"
    "${SHADERS_DIR}/compute" "${SHADERS_DIR}/compute/collision.spv"
    -entry initialize_lbm -entry collide)
compile_slang_shaders(
    streaming_shader "shaders/compute/streaming.slang"
    "${SHADERS_DIR}/compute" "${SHADERS_DIR}/compute/streaming.spv"
    -entry calculate_streaming -entry apply_streaming)
compile_slang_shaders(
    transitions_shader "shaders/compute/transitions.slang"
    "${SHADERS_DIR}/compute" "${SHADERS_DIR}/compute/transitions.spv"
    -entry mark_transitions -entry update_fluid_transition_neighbors
    -entry apply_gas_to_interface -entry update_gas_transition_neighbors
    -entry calculate_excess_distribution -entry apply_excess_distribution
    -entry apply_transitions -entry remove_dam
    -entry reclassify_after_dam_removal)
add_dependencies(rheo-lbm collision_shader streaming_shader
    transitions_shader)
if(TARGET lattice_gpu_test)
    add_dependencies(lattice_gpu_test lattice_edit_shader)
    add_dependencies(lattice_gpu_test collision_shader streaming_shader
        transitions_shader)
    add_dependencies(terrain_render_smoke lattice_edit_shader)
endif()
