add_library(
    rheo_simulation STATIC
    src/shallow_water_solver.cpp
    src/shallow_water_session.cpp
    src/lattice_initializer.cpp
    src/lattice_editor.cpp
    src/free_surface_solver.cpp
    src/free_surface_session.cpp
)
add_library(rheo::simulation ALIAS rheo_simulation)
target_include_directories(rheo_simulation PUBLIC include PRIVATE src)
target_link_libraries(
    rheo_simulation
    PUBLIC rheo::domain rheo::graphics
)

find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(rheo_simulation PUBLIC OpenMP::OpenMP_CXX)
endif()
