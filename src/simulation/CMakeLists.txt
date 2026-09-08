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
