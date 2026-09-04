add_library(
    rheo_graphics STATIC
    src/buffer.cpp
    src/command_pool.cpp
    src/descriptor.cpp
    src/frame_sync.cpp
    src/images.cpp
    src/immediate_submit.cpp
    src/memory.cpp
    src/pipeline.cpp
    src/context.cpp
    src/device.cpp
    src/swap_chain.cpp
)
add_library(rheo::graphics ALIAS rheo_graphics)
target_include_directories(rheo_graphics PUBLIC include)
target_link_libraries(
    rheo_graphics
    PUBLIC rheo_graphics_backend rheo::domain rheo::platform
    PRIVATE glfw
)
