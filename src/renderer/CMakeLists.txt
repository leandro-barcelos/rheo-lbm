add_library(
    rheo_renderer STATIC
    src/camera.cpp
    src/lattice_renderer.cpp
    src/renderer.cpp
)
add_library(rheo::renderer ALIAS rheo_renderer)
target_include_directories(rheo_renderer PUBLIC include PRIVATE src)
target_link_libraries(
    rheo_renderer
    PUBLIC rheo::application rheo::events rheo::graphics rheo::platform
    PRIVATE rheo::simulation
)
