if(RHEO_MODEL STREQUAL "2D")
    find_package(OpenMP QUIET)
    add_library(rheo_simulation STATIC src/labswe_solver.cpp src/shallow_water_session.cpp)
    add_library(rheo::simulation ALIAS rheo_simulation)
    target_include_directories(rheo_simulation PUBLIC include)
    target_link_libraries(rheo_simulation PUBLIC rheo::domain)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(rheo_simulation PUBLIC OpenMP::OpenMP_CXX)
    endif()
    return()
endif()

add_library(
    rheo_simulation STATIC
    src/lattice_initializer.cpp
    src/lattice_editor.cpp
    src/lbm_solver.cpp
    src/simulation_session.cpp
)
add_library(rheo::simulation ALIAS rheo_simulation)
target_include_directories(rheo_simulation PUBLIC include PRIVATE src)
target_link_libraries(
    rheo_simulation
    PUBLIC rheo::domain rheo::graphics
)
