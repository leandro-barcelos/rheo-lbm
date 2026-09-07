#include "geotiff.h"

#include <gdal_priv.h>
#include <ogr_spatialref.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

namespace assets {
domain::DemData ReadGeoTiff(std::string const& path) {
  GDALAllRegister();
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> source(
      static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly)),
      GDALClose);
  if (!source) throw std::runtime_error("Could not open DEM: " + path);
  int const width = source->GetRasterXSize(), height = source->GetRasterYSize();
  if (width <= 0 || height <= 0 || source->GetRasterCount() < 1)
    throw std::runtime_error("DEM dimensions are invalid");
  std::array<double, 6> transform{};
  if (source->GetGeoTransform(transform.data()) != CE_None ||
      !std::ranges::all_of(transform,
                           [](double v) { return std::isfinite(v); }) ||
      transform[1] == 0 || transform[5] == 0)
    throw std::runtime_error("DEM has no usable geographic pixel scale");
  if (transform[2] != 0 || transform[4] != 0)
    throw std::runtime_error("Rotated DEM transforms are not supported");
  auto const* srs = source->GetSpatialRef();
  if (!srs)
    throw std::runtime_error("DEM coordinate reference system is missing");
  double scale_x = 0, scale_z = 0;
  if (srs->IsProjected()) {
    scale_x = scale_z = srs->GetLinearUnits();
  } else if (srs->IsGeographic()) {
    constexpr double radius = 6378137.0;
    double const radians_per_unit = srs->GetAngularUnits();
    double const latitude =
        (transform[3] + height * 0.5 * transform[5]) * radians_per_unit;
    if (std::abs(latitude) >= 1.5707963267948966)
      throw std::runtime_error(
          "DEM center latitude is outside the supported range");
    scale_z = radius * radians_per_unit;
    scale_x = scale_z * std::cos(latitude);
  } else {
    throw std::runtime_error("Unsupported DEM coordinate reference system");
  }
  domain::DemData dem;
  dem.width = static_cast<std::uint32_t>(width);
  dem.height = static_cast<std::uint32_t>(height);
  dem.pixel_size_meters = {std::abs(transform[1]) * scale_x,
                           std::abs(transform[5]) * scale_z};
  auto* band = source->GetRasterBand(1);
  // Elevation samples are meters unless the band explicitly declares feet.
  std::string const unit = band->GetUnitType();
  double elevation_unit = 1;
  if (unit == "ft" || unit == "foot" || unit == "feet")
    elevation_unit = 0.3048;
  else if (unit == "US survey foot" || unit == "us_survey_foot")
    elevation_unit = 1200.0 / 3937.0;
  else if (!unit.empty() && unit != "m" && unit != "meter" && unit != "metre")
    throw std::runtime_error("Unsupported DEM elevation unit: " + unit);
  int has_nodata = 0;
  double const nodata = band->GetNoDataValue(&has_nodata);
  double const offset = band->GetOffset(), scale = band->GetScale();
  std::vector<double> values(static_cast<std::size_t>(width) * height);
  if (band->RasterIO(GF_Read, 0, 0, width, height, values.data(), width, height,
                     GDT_Float64, 0, 0) != CE_None)
    throw std::runtime_error("Could not read DEM elevations");
  std::vector<unsigned char> mask(values.size(), 255);
  if (!(band->GetMaskFlags() & GMF_ALL_VALID) &&
      band->GetMaskBand()->RasterIO(GF_Read, 0, 0, width, height, mask.data(),
                                    width, height, GDT_Byte, 0, 0) != CE_None)
    throw std::runtime_error("Could not read DEM validity mask");
  dem.samples.resize(values.size());
  dem.min_elevation = std::numeric_limits<float>::infinity();
  dem.max_elevation = -dem.min_elevation;
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      auto const i = static_cast<std::size_t>(row) * width + col;
      int const x = transform[1] > 0 ? col : width - 1 - col;
      int const z = transform[5] > 0 ? row : height - 1 - row;
      float value =
          static_cast<float>((values[i] * scale + offset) * elevation_unit);
      if (!mask[i] || !std::isfinite(values[i]) ||
          (has_nodata && values[i] == nodata) || !std::isfinite(value))
        value = std::numeric_limits<float>::quiet_NaN();
      else {
        dem.min_elevation = std::min(dem.min_elevation, value);
        dem.max_elevation = std::max(dem.max_elevation, value);
      }
      dem.samples[static_cast<std::size_t>(z) * width + x] = {
          .coordinate = {x, z}, .elevation = value, .pad0 = 0};
    }
  }
  if (!dem.IsValid())
    throw std::runtime_error("DEM has invalid scale or no valid elevations");
  for (auto& sample : dem.samples)
    if (!std::isfinite(sample.elevation)) sample.elevation = dem.min_elevation;
  return dem;
}
}  // namespace assets
