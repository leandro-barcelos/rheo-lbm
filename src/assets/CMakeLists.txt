add_library(
    rheo_assets STATIC
    src/geotiff.cpp
    src/image_loader.cpp
    src/project_repository.cpp
    src/dem_loader.cpp
)
add_library(rheo::assets ALIAS rheo_assets)
target_include_directories(rheo_assets PUBLIC include PRIVATE src)
target_include_directories(rheo_assets PRIVATE ${PROJECT_SOURCE_DIR}/external)
target_link_libraries(
    rheo_assets
    PUBLIC rheo::domain
    PRIVATE GDAL::GDAL yaml-cpp::yaml-cpp
)
