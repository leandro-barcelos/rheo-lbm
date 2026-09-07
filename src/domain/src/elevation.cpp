#include "rheo/domain/elevation.h"

#include <cstdint>

float domain::Elevation::GetElevation(
    std::vector<Elevation> const& elevation_samples, uint32_t elevation_width,
    uint32_t elevation_height, glm::vec2 const& coordinate) {
  if (elevation_width == 0 || elevation_height == 0 ||
      elevation_samples.empty()) {
    return 0.0F;
  }

  if (coordinate[0] < 0 ||
      static_cast<uint32_t>(coordinate[0]) > elevation_width ||
      coordinate[1] < 0 ||
      static_cast<uint32_t>(coordinate[1]) > elevation_height) {
    return 0.0F;
  }

  auto sample_index = static_cast<uint32_t>(coordinate[0]) +
                      (static_cast<uint32_t>(coordinate[1]) * elevation_width);
  return elevation_samples[sample_index].elevation;
}
