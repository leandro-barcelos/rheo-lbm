#include "rheo/domain/lattice_definition.h"

#include <algorithm>
#include <cmath>
#include <limits>
namespace domain {
std::expected<LatticeDefinition, std::string> DefineLattice(
    DemData const& dem, LatticeSettings const& settings) {
  if (auto valid = ValidateLatticeSettings(settings); !valid)
    return std::unexpected(valid.error());
  if (!dem.IsValid()) return std::unexpected("Invalid DEM");
  double const range =
      static_cast<double>(dem.max_elevation) - dem.min_elevation;
  if (!std::isfinite(static_cast<float>(range)))
    return std::unexpected(
        "DEM elevation range exceeds GPU floating-point range");
  float const cell =
      range > 0
          ? static_cast<float>(range / settings.height_subdivisions)
          : std::max({dem.pixel_size_meters.x, dem.pixel_size_meters.y, 1.0F});
  if (!std::isfinite(cell) || cell <= 0)
    return std::unexpected("Invalid lattice cell size");
  double const x = std::max(1.0, std::ceil(static_cast<double>(dem.width) *
                                           dem.pixel_size_meters.x / cell));
  double const y =
      settings.height_subdivisions +
      std::ceil(static_cast<double>(settings.upper_elevation_margin) / cell);
  double const z = std::max(1.0, std::ceil(static_cast<double>(dem.height) *
                                           dem.pixel_size_meters.y / cell));
  constexpr auto limit = std::numeric_limits<std::uint32_t>::max();
  if (!std::isfinite(x * y * z) || x * y * z > limit)
    return std::unexpected(
        "Lattice contains too many cells; reduce height subdivisions or "
        "margin");
  return LatticeDefinition{static_cast<std::uint32_t>(x),
                           static_cast<std::uint32_t>(y),
                           static_cast<std::uint32_t>(z),
                           static_cast<std::uint32_t>(x * y * z),
                           cell,
                           static_cast<float>(range / cell)};
}
}  // namespace domain
