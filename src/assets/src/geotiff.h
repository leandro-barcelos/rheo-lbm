#ifndef RHEOLBM_GEOTIFF_H
#define RHEOLBM_GEOTIFF_H

#include <gdal.h>
#include <gdal_dataset.h>

#include <array>
#include <string>
#include <vector>

#include "rheo/domain/elevation.h"

namespace assets {

class GeoTiff {
 public:
  GeoTiff(const GeoTiff&) = delete;
  GeoTiff(GeoTiff&&) = delete;
  GeoTiff& operator=(const GeoTiff&) = delete;
  GeoTiff& operator=(GeoTiff&&) = delete;
  explicit GeoTiff(std::string filename);
  ~GeoTiff();

  [[nodiscard]] std::array<int, 3> const& Dimensions() const {
    return dimensions_;
  }
  [[nodiscard]] std::vector<domain::Elevation> Elevations(
      int layer = 1, float resolution_meters = 10.0F);

 private:
  std::string filename_;
  GDALDataset* geotiff_dataset_ = nullptr;
  std::array<int, 3> dimensions_{0};
  std::array<double, 6> geo_transform_{0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  bool has_geo_transform_ = false;
};

}  // namespace assets

#endif  // !RHEOLBM_GEOTIFF_H
