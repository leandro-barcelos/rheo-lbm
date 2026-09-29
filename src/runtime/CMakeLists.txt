add_library(rheo_runtime STATIC
    src/application_backend.cpp
)
add_library(rheo::runtime ALIAS rheo_runtime)
target_include_directories(rheo_runtime PUBLIC include)
target_link_libraries(
    rheo_runtime
    PUBLIC rheo::renderer rheo::assets rheo::ui rheo::events rheo::platform
     rheo::graphics
)
