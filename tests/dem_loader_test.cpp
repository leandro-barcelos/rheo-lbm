#include <cpl_vsi.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "rheo/assets/asset_services.h"
void Check(bool value) {
  if (!value) throw std::runtime_error("DEM loader check failed");
}
void Write(char const* path, int epsg, std::array<double, 6> transform,
           std::array<float, 4> values) {
  auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  auto* data = driver->Create(path, 2, 2, 1, GDT_Float32, nullptr);
  Check(data != nullptr);
  if (epsg) {
    OGRSpatialReference srs;
    Check(srs.importFromEPSG(epsg) == OGRERR_NONE);
    Check(data->SetSpatialRef(&srs) == CE_None);
  }
  Check(data->SetGeoTransform(transform.data()) == CE_None);
  auto* band = data->GetRasterBand(1);
  Check(band->SetNoDataValue(-9999) == CE_None);
  Check(band->RasterIO(GF_Write, 0, 0, 2, 2, values.data(), 2, 2, GDT_Float32,
                       0, 0) == CE_None);
  GDALClose(data);
}
int main() {
  GDALAllRegister();
  assets::DemLoader loader;
  char const* path = "/vsimem/rheo-dem-test.tif";
  Write(path, 32623, {0, 10, 0, 0, 0, -20}, {1, 2, 3, -9999});
  auto dem = loader.Load(path);
  Check(dem.has_value());
  Check((*dem)->pixel_size_meters.x == 10 && (*dem)->pixel_size_meters.y == 20);
  Check((*dem)->min_elevation == 1 && (*dem)->max_elevation == 3);
  Check((*dem)->samples[0].elevation == 3 &&
        (*dem)->samples[1].elevation == 1 &&
        (*dem)->samples[2].elevation == 1 && (*dem)->samples[3].elevation == 2);
  Write(path, 4326, {-45, 0.001, 0, -20, 0, -0.001}, {4, 4, 4, 4});
  dem = loader.Load(path);
  Check(dem.has_value());
  Check(std::abs((*dem)->pixel_size_meters.y - 111.31949F) < 0.001F);
  Check((*dem)->pixel_size_meters.x > 104 && (*dem)->pixel_size_meters.x < 105);
  Check((*dem)->min_elevation == 4 && (*dem)->max_elevation == 4);
  Write(path, 2277, {0, 10, 0, 0, 0, 10}, {1, 2, 3, 4});
  dem = loader.Load(path);
  Check(dem.has_value());
  Check(std::abs((*dem)->pixel_size_meters.x - 3.048006F) < 0.0001F);
  Check((*dem)->samples[0].elevation == 1);
  Write(path, 32623, {0, -10, 0, 0, 0, -10}, {1, 2, 3, 4});
  dem = loader.Load(path);
  Check(dem.has_value());
  Check((*dem)->samples[0].elevation == 4);
  Write(path, 32623, {0, 10, 0, 0, 0, -10},
        {-9999, std::numeric_limits<float>::quiet_NaN(), -9999, -9999});
  Check(!loader.Load(path));
  Write(path, 32623, {0, 10, 1, 0, 0, -10}, {1, 2, 3, 4});
  Check(!loader.Load(path));
  Write(path, 0, {0, 10, 0, 0, 0, -10}, {1, 2, 3, 4});
  Check(!loader.Load(path));
  Check(!loader.Load(""));
  VSIUnlink(path);
}
