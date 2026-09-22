if(RHEO_MODEL STREQUAL "2D")
    add_library(rheo_domain STATIC src/shallow_water.cpp)
    add_library(rheo::domain ALIAS rheo_domain)
    target_include_directories(rheo_domain PUBLIC include)
    target_link_libraries(rheo_domain PUBLIC glm::glm)
    return()
endif()

add_library(rheo_domain STATIC src/elevation.cpp
    src/lattice_definition.cpp
    src/lattice_editing.cpp)
add_library(rheo::domain ALIAS rheo_domain)
target_include_directories(rheo_domain PUBLIC include)
target_link_libraries(rheo_domain PUBLIC glm::glm)
