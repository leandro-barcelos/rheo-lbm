#ifndef RHEO_DOMAIN_TERRAIN_DATA_H
#define RHEO_DOMAIN_TERRAIN_DATA_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "rheo/domain/elevation.h"

namespace domain {

struct TerrainData {
  std::vector<Elevation> samples;
  std::uint32_t width = 0;
  std::uint32_t height = 0;

  [[nodiscard]] bool IsValid() const {
    return width > 0 && height > 0 &&
           samples.size() == static_cast<std::size_t>(width) * height;
  }
};

using SharedTerrain = std::shared_ptr<const TerrainData>;

}  // namespace domain

#endif  // RHEO_DOMAIN_TERRAIN_DATA_H
