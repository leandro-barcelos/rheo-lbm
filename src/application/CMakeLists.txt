add_library(rheo_application STATIC src/application_controller.cpp)
add_library(rheo::application ALIAS rheo_application)
target_include_directories(rheo_application PUBLIC include)
target_link_libraries(
    rheo_application
    PUBLIC rheo::domain rheo::assets rheo::simulation
)
