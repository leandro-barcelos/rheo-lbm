if(RHEO_MODEL STREQUAL "2D")
    add_library(rheo_application STATIC src/shallow_water_controller.cpp)
    add_library(rheo::application ALIAS rheo_application)
    target_include_directories(rheo_application PUBLIC include)
    target_link_libraries(rheo_application PUBLIC rheo::domain rheo::simulation rheo::events)
    return()
endif()

add_library(rheo_application STATIC
    src/application_controller.cpp
    src/lattice_brush_controller.cpp
    src/editor_input_router.cpp
)
add_library(rheo::application ALIAS rheo_application)
target_include_directories(rheo_application PUBLIC include)
target_link_libraries(
    rheo_application
    PUBLIC rheo::domain rheo::assets rheo::simulation rheo::events
)
