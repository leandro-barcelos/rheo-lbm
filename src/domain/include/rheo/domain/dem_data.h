#ifndef RHEO_DOMAIN_DEM_DATA_H
#define RHEO_DOMAIN_DEM_DATA_H
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "rheo/domain/elevation.h"
namespace domain {
struct DemData {
  // Row-major, X eastwards, Z northwards; heights are meters.
  std::vector<Elevation> samples;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  glm::vec2 pixel_size_meters{0.0F};
  float min_elevation = 0;
  float max_elevation = 0;
  [[nodiscard]] bool IsValid() const {
    return width > 0 && height > 0 &&
           samples.size() == static_cast<std::size_t>(width) * height &&
           std::isfinite(pixel_size_meters.x) &&
           std::isfinite(pixel_size_meters.y) && pixel_size_meters.x > 0 &&
           pixel_size_meters.y > 0 && std::isfinite(min_elevation) &&
           std::isfinite(max_elevation) && max_elevation >= min_elevation;
  }
};
using SharedDem = std::shared_ptr<const DemData>;
}  // namespace domain
#endif
