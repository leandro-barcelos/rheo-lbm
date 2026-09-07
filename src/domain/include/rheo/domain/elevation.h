#ifndef RHEOLBM_ELEVATION_H
#define RHEOLBM_ELEVATION_H

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace domain {

struct Elevation {  // NOLINT(altera-struct-pack-align)
  glm::vec2 coordinate;
  float elevation;
  float pad0;

  [[nodiscard]] static float GetElevation(
      std::vector<Elevation> const& elevation_samples, uint32_t elevation_width,
      uint32_t elevation_height, glm::vec2 const& coordinate);
} __attribute__((aligned(16)));

}  // namespace domain

static_assert(sizeof(domain::Elevation) == 16);

#endif  // !RHEOLBM_ELEVATION_H
