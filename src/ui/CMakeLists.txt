if(RHEO_MODEL STREQUAL "2D")
    add_library(rheo_ui STATIC src/imgui_layer.cpp src/shallow_water_interface.cpp)
    add_library(rheo::ui ALIAS rheo_ui)
    target_include_directories(rheo_ui PUBLIC include PRIVATE src ${PROJECT_SOURCE_DIR}/external)
    target_link_libraries(rheo_ui PUBLIC rheo::application rheo::graphics rheo::platform rheo::renderer PRIVATE imgui)
    return()
endif()

add_library(
    rheo_ui STATIC
    src/imgui_layer.cpp
    src/user_interface.cpp
    src/panels/control_panel.cpp
    src/panels/parameters_panel.cpp
)
add_library(rheo::ui ALIAS rheo_ui)
target_include_directories(rheo_ui PUBLIC include PRIVATE src)
target_include_directories(rheo_ui PRIVATE ${PROJECT_SOURCE_DIR}/external)
target_link_libraries(
    rheo_ui
    PUBLIC rheo::application rheo::graphics rheo::platform rheo::renderer
    PRIVATE imgui
)
