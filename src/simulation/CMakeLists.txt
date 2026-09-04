add_library(
    rheo_simulation STATIC
    src/fluid_simulator.cpp
    src/simulation_session.cpp
)
add_library(rheo::simulation ALIAS rheo_simulation)
target_include_directories(rheo_simulation PUBLIC include PRIVATE src)
target_link_libraries(
    rheo_simulation
    PUBLIC rheo::domain rheo::graphics
)
