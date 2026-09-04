add_library(rheo_domain STATIC src/elevation.cpp)
add_library(rheo::domain ALIAS rheo_domain)
target_include_directories(rheo_domain PUBLIC include)
target_link_libraries(rheo_domain PUBLIC glm::glm)
